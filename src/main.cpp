#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QByteArray>

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

#include "Types.h"
#include "Camera.h"
#include "Scene.h"
#include "Renderer.h"

#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <algorithm>
#include <random>
#include <iostream>

const int SAMPLES_PER_PIXEL = 512;
const int MAX_BOUNCES = 6;

class RenderWidget : public QWidget {
public:
    RenderWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setBaseSize(width, height);

        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);

        updateBuffers();
        restartRender();

        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); updateCamera(); });
        timer->start(0);
    }

    ~RenderWidget() {
        cancelRender = true;
    }

protected:
    void updateCamera() {
        if (downKeys.size() == 0) {
            if (isMoving) {
                isMoving = false;
                resetAndRestart();
            }
            return;
        }

        isMoving = true;

        glm::vec3 right = glm::normalize(glm::cross(camera.forward, glm::vec3(0, 1, 0)));

        if (std::find(downKeys.begin(), downKeys.end(), Qt::Key_W) != downKeys.end())
            camera.position += camera.forward * camera.speed;
        if (std::find(downKeys.begin(), downKeys.end(), Qt::Key_S) != downKeys.end())
            camera.position -= camera.forward * camera.speed;
        if (std::find(downKeys.begin(), downKeys.end(), Qt::Key_A) != downKeys.end())
            camera.position -= right * camera.speed;
        if (std::find(downKeys.begin(), downKeys.end(), Qt::Key_D) != downKeys.end())
            camera.position += right * camera.speed;

        resetAndRestart();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawImage(0, 0, image);
    }

    void resizeEvent(QResizeEvent* event) override {
        cancelRender = true;

        if (renderFuture.valid())
            renderFuture.wait();

        width = event->size().width();
        height = event->size().height();

        updateBuffers();

        cancelRender = false;

        restartRender();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (std::find(downKeys.begin(), downKeys.end(), event->key()) != downKeys.end())
            return;
        downKeys.push_back(event->key());
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        auto e = std::find(downKeys.begin(), downKeys.end(), event->key());
        if (e == downKeys.end())
            return;
        downKeys.erase(e);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::RightButton) {
            isLooking = true;

            QPoint delta = event->pos() - lastMousePos;
            lastMousePos = event->pos();

            yaw += delta.x() * 0.2f;
            pitch -= delta.y() * 0.2f;
            pitch = glm::clamp(pitch, -89.9f, 89.9f);

            glm::vec3 front;
            front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            front.y = sin(glm::radians(pitch));
            front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            camera.forward = glm::normalize(front);

            resetAndRestart();
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        lastMousePos = event->pos();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (isLooking) {
            isLooking = false;
            resetAndRestart();
        }
    }

private:
    int width = 800;
    int height = 800;

    uint32_t seed = 12345;

    Camera camera;

    float yaw = -90.0f;
    float pitch = 0.0f;

    QPoint lastMousePos;

    std::vector<int> downKeys;

    std::atomic<int> nextPixel{0};

    std::vector<float> pixels;
    std::vector<glm::vec3> accumBuffer;

    QImage image;

    std::future<void> renderFuture;

    std::atomic<bool> isMoving{false};
    std::atomic<bool> isLooking{false};

    std::atomic<int> renderId{0};
    std::atomic<bool> cancelRender{false};

    void updateBuffers() {
        pixels.assign(width * height * 3, 0.0f);
        accumBuffer.assign(width * height, glm::vec3(0.0f));

        image = QImage(width, height, QImage::Format_RGB32);
        image.detach();
    }

    void resetAndRestart() {
        std::fill(pixels.begin(), pixels.end(), 0.0f);
        std::fill(accumBuffer.begin(), accumBuffer.end(), glm::vec3(0.0f));

        restartRender();
    }

    void restartRender() {
        renderId++;
        renderFuture = std::async(std::launch::async, &RenderWidget::renderCPU, this);
    }

    bool isPerformanceMode() {
        return isMoving || isLooking;
    }

    float fastRand(uint32_t& s) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (s & 0xFFFFFF) / float(0x1000000);
    }

    void renderCPU() {
        int currentRenderId = renderId;

        int numThreads = std::thread::hardware_concurrency();

        bool performanceMode = isPerformanceMode();

        int maxSamples = performanceMode ? 1 : SAMPLES_PER_PIXEL;

        uchar* imgData = image.bits();
        int bytesPerLine = image.bytesPerLine();

        std::vector<int> pixelOrder(width * height);
        for (int i = 0; i < pixelOrder.size(); ++i)
            pixelOrder[i] = i;

        thread_local std::mt19937 randomEngine(std::random_device{}());
        std::shuffle(pixelOrder.begin(), pixelOrder.end(), randomEngine);

        for (int s = 1; s <= maxSamples; ++s) {
            if (renderId != currentRenderId || cancelRender)
                return;

            std::vector<std::thread> threads;

            nextPixel = 0;

            for (int t = 0; t < numThreads; ++t) {
                threads.emplace_back([this, s, performanceMode, &pixelOrder, imgData, bytesPerLine]() {
                    workerPass(s, performanceMode, pixelOrder, imgData, bytesPerLine);
                });
            }

            for (auto& th : threads)
                th.join();
        }
    }

    void workerPass(int sampleId, bool performanceMode, const std::vector<int>& pixelOrder, uchar* imgData, int bytesPerLine) {
        float aspectRatio = (float)width / (float)height;
        int step = performanceMode ? 8 : 1;

        uint32_t baseSeed = seed ^ sampleId;

        int totalPixels = width * height;

        while (true) {
            if (cancelRender)
                return;

            int i = nextPixel.fetch_add(1);
            if (i >= totalPixels)
                break;

            int ri = pixelOrder[i];

            int x = ri % width;
            int y = ri / width;

            if ((x % step) || (y % step))
                continue;

            uint32_t localSeed = baseSeed ^ (ri * 9781);

            float jitterX = performanceMode ? 0.5f : fastRand(localSeed);
            float jitterY = performanceMode ? 0.5f : fastRand(localSeed);

            float u = (x + jitterX) / width;
            float v = (y + jitterY) / height;

            Ray ray = camera.getRay(u, v, aspectRatio);
            glm::vec3 sampleColor = getRayColor(ray, 0, performanceMode ? 1 : MAX_BOUNCES);

            if (performanceMode) {
                for (int dy = 0; dy < step; ++dy) {
                    int py = y + dy;
                    if (py >= height)
                        continue;

                    QRgb* row = reinterpret_cast<QRgb*>(imgData + py * bytesPerLine);

                    for (int dx = 0; dx < step; ++dx) {
                        int px = x + dx;
                        if (px >= width)
                            continue;

                        int r = int(glm::clamp(sampleColor.r, 0.0f, 1.0f) * 255.0f);
                        int g = int(glm::clamp(sampleColor.g, 0.0f, 1.0f) * 255.0f);
                        int b = int(glm::clamp(sampleColor.b, 0.0f, 1.0f) * 255.0f);

                        row[px] = qRgb(r, g, b);
                    }
                }
            } else {
                int pixelIndex = y * width + x;
                accumBuffer[pixelIndex] += sampleColor;

                glm::vec3 avg = glm::sqrt(accumBuffer[pixelIndex] / (float)sampleId);

                QRgb* row = reinterpret_cast<QRgb*>(imgData + y * bytesPerLine);

                int r = int(glm::clamp(avg.r, 0.0f, 1.0f) * 255.0f);
                int g = int(glm::clamp(avg.g, 0.0f, 1.0f) * 255.0f);
                int b = int(glm::clamp(avg.b, 0.0f, 1.0f) * 255.0f);

                row[x] = qRgb(r, g, b);
            }
        }
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    RenderWidget window;
    window.setWindowTitle("L Tracer");
    window.show();

    for (auto& box : scene) {
        auto m = box.transform.getModelMatrix();
        box.inverseModel = glm::inverse(m);
        box.normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
    }

    return app.exec();
}