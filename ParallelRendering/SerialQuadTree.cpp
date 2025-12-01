#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <chrono>
#include <algorithm>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <mutex>

#include "SFML/Graphics.hpp"

// 1. 기본 구조체
struct Point {
    float x, y;
};

struct Bounds {
    float x, y;
    float width, height;

    bool contains(Point p) const {
        return (p.x >= x && p.x <= x + width &&
            p.y >= y && p.y <= y + height);
    }

    bool intersects(const Bounds& other) const {
        return !(other.x > x + width ||
            other.x + other.width < x ||
            other.y > y + height ||
            other.y + other.height < y);
    }
};

struct GameObject {
    int id;
    Bounds bounds;
    Point velocity;
    sf::CircleShape shape;

    GameObject(int id_val, Bounds b, Point vel)
        : id(id_val), bounds(b), velocity(vel) {

        float radius = bounds.width / 2.0f;
        shape.setRadius(radius);
        shape.setOrigin({ radius, radius });
        shape.setPosition({ bounds.x + radius, bounds.y + radius });
        shape.setFillColor(sf::Color::White);
    }

    void update(float dt, float mapWidth, float mapHeight) {
        bounds.x += velocity.x * dt;
        bounds.y += velocity.y * dt;

        if (bounds.x < 0 || bounds.x + bounds.width > mapWidth) {
            velocity.x *= -1;
            bounds.x = std::max(0.0f, std::min(bounds.x, mapWidth - bounds.width));
        }
        if (bounds.y < 0 || bounds.y + bounds.height > mapHeight) {
            velocity.y *= -1;
            bounds.y = std::max(0.0f, std::min(bounds.y, mapHeight - bounds.height));
        }

        float radius = bounds.width / 2.0f;
        shape.setPosition({ bounds.x + radius, bounds.y + radius });
    }
};

// 2. 쿼드트리
class QuadTree {
private:
    struct Node {
        Bounds bounds;
        int capacity;
        bool isLeaf = true;
        int depth;
        static const int MAX_DEPTH = 8;

        std::vector<GameObject*> objects;
        std::unique_ptr<Node> children[4];

        Node(Bounds b, int cap, int d) : bounds(b), capacity(cap), depth(d) {}

        void subdivide() {
            float subWidth = bounds.width / 2.0f;
            float subHeight = bounds.height / 2.0f;
            float x = bounds.x;
            float y = bounds.y;

            children[0] = std::make_unique<Node>(Bounds{ x, y, subWidth, subHeight }, capacity, depth + 1);
            children[1] = std::make_unique<Node>(Bounds{ x + subWidth, y, subWidth, subHeight }, capacity, depth + 1);
            children[2] = std::make_unique<Node>(Bounds{ x, y + subHeight, subWidth, subHeight }, capacity, depth + 1);
            children[3] = std::make_unique<Node>(Bounds{ x + subWidth, y + subHeight, subWidth, subHeight }, capacity, depth + 1);

            isLeaf = false;
        }

        void insert(GameObject* obj) {
            if (!bounds.intersects(obj->bounds)) return;
            if (!isLeaf) {
                for (int i = 0; i < 4; ++i) {
                    if (children[i]->bounds.intersects(obj->bounds)) {
                        children[i]->insert(obj);
                    }
                }
                return;
            }
            objects.push_back(obj);
            if (objects.size() > capacity && depth < MAX_DEPTH) {
                subdivide();
                for (GameObject* objToMove : objects) {
                    for (int i = 0; i < 4; ++i) {
                        if (children[i]->bounds.intersects(objToMove->bounds)) {
                            children[i]->insert(objToMove);
                        }
                    }
                }
                objects.clear();
            }
        }

        void query(const Bounds& range, std::vector<GameObject*>& found) const {
            if (!bounds.intersects(range)) return;
            if (isLeaf) {
                for (GameObject* obj : objects) {
                    if (range.intersects(obj->bounds)) {
                        found.push_back(obj);
                    }
                }
            }
            else {
                for (int i = 0; i < 4; ++i) {
                    children[i]->query(range, found);
                }
            }
        }

        void draw(sf::RenderWindow& window) {
            sf::RectangleShape rect;
            rect.setPosition({ bounds.x, bounds.y });
            rect.setSize({ bounds.width, bounds.height });
            rect.setOutlineColor(sf::Color(64, 64, 64));
            rect.setOutlineThickness(1.0f);
            rect.setFillColor(sf::Color::Transparent);
            window.draw(rect);

            if (!isLeaf) {
                for (int i = 0; i < 4; ++i) {
                    children[i]->draw(window);
                }
            }
        }
    };

    std::unique_ptr<Node> root;
    int capacity;

public:
    QuadTree(Bounds boundary, int cap)
        : capacity(cap) {
        root = std::make_unique<Node>(boundary, cap, 0);
    }

    void insert(GameObject* obj) { root->insert(obj); }

    std::vector<GameObject*> query(const Bounds& range) const {
        std::vector<GameObject*> found;
        root->query(range, found);
        return found;
    }

    void clear() {
        Bounds rootBounds = root->bounds;
        root = std::make_unique<Node>(rootBounds, capacity, 0);
    }

    void draw(sf::RenderWindow& window) {
        if (root) { root->draw(window); }
    }
};

// 3. 유틸리티 및 전역 설정
bool checkCollision(const GameObject& go1, const GameObject& go2) {
    return go1.bounds.intersects(go2.bounds);
}

sf::Text makeText(sf::Font& font, int char_size, sf::Color color) {
    sf::Text text(font);
    text.setCharacterSize(char_size);
    text.setFillColor(color);
    text.setPosition({ 10.f, 10.f });
    text.setStyle(sf::Text::Bold);
    return text;
}

// 동기화 모드 정의
enum class SyncMode {
    SingleThread,  // [0] 순차 처리
    CoarseGrained, // [1] 전역 락
    FineGrained,   // [2] 객체별 락
    LockFree       // [3] 락 프리 (Atomic)
};

// [강제 부하] 성능 차이 체감을 위해 연산량을 늘림 (충돌 시 연산량이 많을 경우)
void heavy_physics_simulation() {
    volatile int dummy = 0;
    // 이 반복 횟수가 높을수록 싱글 스레드와 멀티 스레드의 차이,
    // 락 유무에 따른 차이가 극명해짐
    for (int k = 0; k < 30000; ++k) {
        dummy += k;
    }
}

// 4. 메인 함수
int main() {
    const int WIN_WIDTH = 1800;
    const int WIN_HEIGHT = 1200;
    const int NUM_OBJECTS = 10000;
    const int NODE_CAPACITY = 4;

    sf::RenderWindow window(sf::VideoMode({ (unsigned int)WIN_WIDTH, (unsigned int)WIN_HEIGHT }), "Performance Comparison");
    window.setFramerateLimit(60);

    sf::Font font("ARIAL.TTF");
    sf::Text fpsText = makeText(font, 20, sf::Color::Green);

    std::vector<GameObject> allObjects;
    allObjects.reserve(NUM_OBJECTS);

    std::vector<std::atomic_bool> collided(NUM_OBJECTS);

    // 전역 락
    std::mutex globalMutex;

    // 객체별 락
    std::vector<std::unique_ptr<std::mutex>> objectMutexes;
    objectMutexes.reserve(NUM_OBJECTS);
    for (int i = 0; i < NUM_OBJECTS; ++i) {
        objectMutexes.push_back(std::make_unique<std::mutex>());
    }

    // Lock Free
    SyncMode currentMode = SyncMode::LockFree;

    std::mt19937 rng(static_cast<unsigned int>(
        std::chrono::system_clock::now().time_since_epoch().count()
        ));
    std::uniform_real_distribution<float> rand_pos_x(0.0f, (float)WIN_WIDTH);
    std::uniform_real_distribution<float> rand_pos_y(0.0f, (float)WIN_HEIGHT);
    std::uniform_real_distribution<float> rand_vel(-50.0f, 50.0f);
    std::uniform_real_distribution<float> rand_radius(3.0f, 6.0f);

    for (int i = 0; i < NUM_OBJECTS; ++i) {
        float radius = rand_radius(rng);
        Bounds b = { rand_pos_x(rng), rand_pos_y(rng), radius * 2, radius * 2 };
        Point vel = { rand_vel(rng), rand_vel(rng) };
        allObjects.emplace_back(i, b, vel);
    }

    QuadTree tree({ 0, 0, (float)WIN_WIDTH, (float)WIN_HEIGHT }, NODE_CAPACITY);

    sf::Clock clock;
    sf::Clock fpsClock;
    int frameCount = 0;
    float fps = 0.f;

    // 스레드 풀
    std::vector<std::thread> thread_collide_tests;
    unsigned int n_thread_collide_test = std::max(1u, std::thread::hardware_concurrency() - 1); // 메인 제외
    thread_collide_tests.reserve(n_thread_collide_test);

    // 워커 스레드
    auto worker_collide_test = [&](std::atomic<size_t>& nextIndex) {
        while (true) {
            size_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if (i >= allObjects.size()) break;

            GameObject& objA = allObjects[i];
            auto candidates = tree.query(objA.bounds);

            for (GameObject* objB : candidates) {
                if (objA.id >= objB->id) continue;

                if (checkCollision(objA, *objB)) {
                    // 병렬 모드별 로직
                    if (currentMode == SyncMode::CoarseGrained) {
                        std::lock_guard<std::mutex> lock(globalMutex);
                        heavy_physics_simulation();
                        collided[objA.id].store(true, std::memory_order_relaxed);
                        collided[objB->id].store(true, std::memory_order_relaxed);
                    }
                    else if (currentMode == SyncMode::FineGrained) {
                        int first = std::min(objA.id, objB->id);
                        int second = std::max(objA.id, objB->id);
                        objectMutexes[first]->lock();
                        objectMutexes[second]->lock();
                        heavy_physics_simulation();
                        collided[objA.id].store(true, std::memory_order_relaxed);
                        collided[objB->id].store(true, std::memory_order_relaxed);
                        objectMutexes[second]->unlock();
                        objectMutexes[first]->unlock();
                    }
                    else {
                        // [Mode 3] Lock Free (Atomic)
                        // 락이 없지만 안전한(Safe) 방식
                        heavy_physics_simulation();
                        collided[objA.id].store(true, std::memory_order_relaxed);
                        collided[objB->id].store(true, std::memory_order_relaxed);
                    }
                }
            }
        }
        };

    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        frameCount++;

        if (fpsClock.getElapsedTime().asSeconds() >= 1.f) {
            fps = frameCount / fpsClock.getElapsedTime().asSeconds();
            frameCount = 0;
            fpsClock.restart();
        }

        std::string modeStr;
        switch (currentMode) {
        case SyncMode::SingleThread:  modeStr = "[0] Single Thread (Slowest)"; break;
        case SyncMode::CoarseGrained: modeStr = "[1] Coarse Grained (Global Lock)"; break;
        case SyncMode::FineGrained:   modeStr = "[2] Fine Grained (Object Lock)"; break;
        case SyncMode::LockFree:      modeStr = "[3] Lock Free (Fastest)"; break;
        }

        std::ostringstream ss;
        ss << "FPS: " << std::fixed << std::setprecision(1) << fps << "\n"
            << "Mode: " << modeStr << "\n"
            << "Press [0], [1], [2], [3] to switch modes";
        fpsText.setString(ss.str());

        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                if (keyEvent->code == sf::Keyboard::Key::Num0) currentMode = SyncMode::SingleThread;
                else if (keyEvent->code == sf::Keyboard::Key::Num1) currentMode = SyncMode::CoarseGrained;
                else if (keyEvent->code == sf::Keyboard::Key::Num2) currentMode = SyncMode::FineGrained;
                else if (keyEvent->code == sf::Keyboard::Key::Num3) currentMode = SyncMode::LockFree;
            }
        }

        for (auto& c : collided) c.store(false, std::memory_order_relaxed);

        for (GameObject& obj : allObjects) {
            obj.update(deltaTime, (float)WIN_WIDTH, (float)WIN_HEIGHT);
            obj.shape.setFillColor(sf::Color::White);
            obj.shape.setOutlineThickness(0);
        }

        tree.clear();
        for (GameObject& obj : allObjects) tree.insert(&obj);


        // 충돌 검사 실행
        if (currentMode == SyncMode::SingleThread) {
            // [0번] 싱글 스레드 처리
            for (size_t i = 0; i < allObjects.size(); ++i) {
                GameObject& objA = allObjects[i];
                auto candidates = tree.query(objA.bounds);

                for (GameObject* objB : candidates) {
                    if (objA.id >= objB->id) continue;

                    if (checkCollision(objA, *objB)) {
                        heavy_physics_simulation(); // 동일한 부하 적용
                        collided[objA.id].store(true, std::memory_order_relaxed);
                        collided[objB->id].store(true, std::memory_order_relaxed);
                    }
                }
            }
        }
        else {
            // [1, 2, 3번] 병렬 처리
            std::atomic<size_t> nextIndex{ 0 };
            for (unsigned int t = 0; t < n_thread_collide_test; ++t) {
                thread_collide_tests.emplace_back(worker_collide_test, std::ref(nextIndex));
            }
            for (auto& th : thread_collide_tests) {
                th.join();
            }
            thread_collide_tests.clear();
        }

        // 렌더링
        for (int i = 0; i < NUM_OBJECTS; ++i) {
            if (collided[i].load(std::memory_order_relaxed)) {
                allObjects[i].shape.setFillColor(sf::Color::Red);
                allObjects[i].shape.setOutlineThickness(1.5f);
                allObjects[i].shape.setOutlineColor(sf::Color::Yellow);
            }
        }

        window.clear(sf::Color::Black);
        tree.draw(window);
        for (const GameObject& obj : allObjects) window.draw(obj.shape);
        window.draw(fpsText);
        window.display();
    }

    return 0;
}