#include <list>
#include <vector>
#include <unistd.h>

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
    Sprite(size_t position) : position(position), age(0) { }

  private:
    size_t position;
    unsigned int age;
    RGBColor color;
};

int main() {
  std::list<Sprite> sprites;
  for (;;) { // Frame loop
    std::vector<Pixel> stripe;
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
    // TODO sleeeeeeep
  }
}
