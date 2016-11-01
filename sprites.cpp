#include <chrono>
#include <cstring>
#include <exception>
#include <list>
#include <netdb.h>
#include <random>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

static const unsigned STR_LEN = 1180;

struct RGBColor {
  unsigned char r, g, b;

  RGBColor& operator+=(const RGBColor& o) {
    r = r + o.r > 255 ? 255 : r + o.r;
    g = g + o.g > 255 ? 255 : g + o.g;
    b = b + o.b > 255 ? 255 : b + o.b;
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
      position--;

      // Age and see if we're still alive
      return age++ <= 100;
    }

    // Starts a new sprite
    Sprite(size_t position, const RGBColor& color) : position(position), age(0), color(color) { }

  private:
    size_t position;
    unsigned int age;
    RGBColor color;
};

std::vector<char> serialize(const std::vector<Pixel>& stripe) {
  std::vector<char> res;
  res.reserve(stripe.size() * 3);
  for (auto& c : stripe) {
    res.push_back(c.color.g);
    res.push_back(c.color.r);
    res.push_back(c.color.b);
  }
  return res;
}

class Sender {
  public:
    Sender(const std::string& hostname) {
      struct addrinfo hints;
      std::memset(&hints,0,sizeof(hints));
      hints.ai_family=AF_UNSPEC;
      hints.ai_socktype=SOCK_DGRAM;
      hints.ai_protocol=0;
      hints.ai_flags=AI_ADDRCONFIG;
      int err=getaddrinfo(hostname.c_str(), "5765", &hints, &ai);
      if (err)
        throw std::runtime_error("failed to resolve remote socket address");

      fd = socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
      if (fd == -1) {
        freeaddrinfo(ai);
        throw std::runtime_error(strerror(errno));
      }
    }

    ~Sender() {
      close(fd);
      freeaddrinfo(ai);
    }

    void send(const std::vector<char>& contents) {
      if (sendto(fd, contents.data(), contents.size(), 0, ai->ai_addr, ai->ai_addrlen) == -1)
        throw std::runtime_error(strerror(errno));
    }

  private:
    struct addrinfo* ai = NULL;
    int fd = 0;
};

int main(int argc, char** argv) {
  if (argc != 2)
    return -1; // TODO maybe print help msg

  const std::string hostname = argv[1];
  Sender sender(hostname);

  std::list<Sprite> sprites;

  // PRNG for inserting new sprites.
  std::random_device r;
  std::default_random_engine e(r());
  std::uniform_int_distribution<int> pos_dist(0, STR_LEN - 1);
  std::uniform_int_distribution<unsigned char> col_dist(0, 255);

  for (;;) { // Frame loop
    // Set up clock so we can sleep at the end of the frame
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(); 

    std::vector<Pixel> stripe(STR_LEN); // Frame buffer

    // Insert new sprites
    sprites.push_back(Sprite(pos_dist(e), RGBColor{col_dist(e), col_dist(e), col_dist(e)}));

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

    sender.send(serialize(stripe));

    std::this_thread::sleep_until(start + std::chrono::milliseconds(80));
  }
}
