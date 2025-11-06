#include <iostream>
#include <vector>
#include "SFML/Graphics.hpp"

constexpr int w = 1024;
constexpr int h = 512;

int main() {
	sf::RenderWindow window(sf::VideoMode({ w,h }), "asdf");
	std::vector<sf::CircleShape> circles(1000000);

	for (auto& circle : circles) {
		circle.setRadius(30.f);
		circle.setFillColor(sf::Color::White);
		circle.setPosition(sf::Vector2f(100, 100));
	}

	const float speed = 200.f;
	sf::Clock clock;

	while (window.isOpen()) {
		sf::Vector2f movement({ 0.f,0.f });
		float dt = clock.restart().asSeconds();

		window.handleEvents(
			[&window](const sf::Event::Closed&) { window.close(); },
			[&](const sf::Event::KeyPressed& k) {
				if (k.scancode == sf::Keyboard::Scancode::Up) movement.y -= speed * dt;
				if (k.scancode == sf::Keyboard::Scancode::Down) movement.y += speed * dt;
				if (k.scancode == sf::Keyboard::Scancode::Left) movement.x -= speed * dt;
				if (k.scancode == sf::Keyboard::Scancode::Right) movement.x += speed * dt;
			}
		);

		window.clear(sf::Color::Black);

		for (auto& circle : circles) {
			circle.move(movement);
			window.draw(circle);
		}
		window.display();
	}
}
