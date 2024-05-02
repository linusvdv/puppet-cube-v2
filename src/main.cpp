#include <SFML/Graphics.hpp>
#include <SFML/Graphics/CircleShape.hpp>


int main (int argc, char *argv[]) {
    sf::RenderWindow window = sf::RenderWindow{ { 1000, 1000 }, "Puppet Cube V2" };
    window.setPosition({0, 0});
    window.setFramerateLimit(60);

    sf::CircleShape circle(100.f);
    circle.setFillColor(sf::Color::Green);

    while (window.isOpen()) {
        for (sf::Event event = sf::Event{}; window.pollEvent(event);) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                window.setView(sf::View(visibleArea));
            }
        }

        window.clear();

        window.draw(circle);

        window.display();
    }
    return 0;
}
