#include <chrono>
#include <list>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>

static const unsigned STR_LEN = 1180;

struct RGBColor {
  unsigned char r, g, b;

  RGBColor& operator+=(const RGBColor& o) {
    r += o.r;
    g += o.g;
    b += o.b;
    return *this;
  }
};

struct Pixel {
  RGBColor color;
};

// Idea: The sprite class could be an abstract superclass / define a common interface to all kinds
// of sprites to be rendered at the same time.
class Sprite {
  public:
    // Renders the sprite onto the stripe buffer.
    void render(std::vector<Pixel>& stripe) const {
      if (position < stripe.size())
        stripe[position].color += color;
    }

    // Call once each frame to update internal data.
    // Returns false if sprite can be removed from scene.
    bool update() {
      // Drift
      position++;

      // Age and see if we're still alive
      return age++ > 100;
    }

    // Starts a new sprite
    Sprite(size_t position) : position(position), age(0), color{255,255,255} { }

  private:
    size_t position;
    unsigned int age;
    RGBColor color;
};

int main() {
  std::list<Sprite> sprites;

  // PRNG for inserting new sprites.
  std::random_device r;
  std::default_random_engine e(r());
  std::uniform_int_distribution<int> dist(0, STR_LEN - 1);

  for (;;) { // Frame loop
    // Set up clock so we can sleep at the end of the frame
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(); 

    std::vector<Pixel> stripe(STR_LEN); // Frame buffer

    // Insert new sprites
    sprites.push_back(Sprite(dist(e)));

    // Render all sprites
    for (auto& sprite : sprites)
      sprite.render(stripe);

    // Update all sprites and remove the ones that died
    for (auto spr_it = sprites.begin(); spr_it != sprites.end(); ) {
      if (spr_it->update())
        spr_it++;
      else
        spr_it = sprites.erase(spr_it);
    }

    // TODO output

    std::this_thread::sleep_until(start + std::chrono::milliseconds(100));
  }
}
