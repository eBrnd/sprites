#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <iostream>
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

  RGBColor& operator*=(const float f) {
    r = r * f;
    g = g * f;
    b = b * f;
    return *this;
  }

  friend RGBColor operator*(const RGBColor& c, float f) {
    RGBColor res = c;
    res *= f;
    return res;
  }
};

struct HSVColor {
  float h, s, v;

  RGBColor to_rgb() const {
    const int hi = h / 60;
    const float f = h / 60 - hi;
    const float p = v * (1 - s);
    const float q = v * (1 - s * f);
    const float t = v * (1 - s * (1 - f));

    float r, g, b;
    switch (hi) {
      case 0: case 6: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = p; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
      default:
        throw std::range_error("hue out of range");
    }

    return RGBColor{
      static_cast<unsigned char>(r * 255),
      static_cast<unsigned char>(g * 255),
      static_cast<unsigned char>(b * 255)
    };
  }
};

struct Pixel {
  RGBColor color;
};

class Sprite {
  public:
    virtual void render(std::vector<Pixel>& stripe) const = 0;
    virtual bool update() = 0;
};

class Pixel_Sprite : public Sprite {
  public:
    // Renders the sprite onto the stripe buffer.
    void render(std::vector<Pixel>& stripe) const {
      float ipos;
      float rpos = std::modf(position, &ipos);
      const size_t pos = ipos;
      if (pos - 1 < stripe.size() && pos - 1 > 0)
        stripe[pos - 1].color += render_color * (1 - rpos);
      if (pos < stripe.size() && pos > 0)
        stripe[pos].color += render_color;
      if (pos + 1 < stripe.size() && pos + 1 > 0)
        stripe[pos + 1].color += render_color * rpos;
    }

    // Call once each frame to update internal data.
    // Returns false if sprite can be removed from scene.
    bool update() {
      // Drift
      if (age > 40)
      {
        if (age < 50)
          velocity += 0.1 * max_velocity;
        position += velocity;
      }

      if (age < 30)
        render_color = color * (age / 30.f);
      else if (age > 100)
        render_color = color *= (200 - age) / 100.f;
      else
        render_color = color;

      // Age and see if we're still alive
      return age++ <= 200;
    }

    // Starts a new sprite
    Pixel_Sprite(size_t position, const RGBColor& color, float max_velocity)
      : position(position), max_velocity(max_velocity), velocity(0), age(0), color(color) { }

  private:
    float position;
    float max_velocity;
    float velocity;
    unsigned int age;
    RGBColor color;
    RGBColor render_color;
};

class Melting : public Sprite {
  public:
    void render(std::vector<Pixel>& stripe) const {
      auto render_color = color;
      render_color.v *= dim();
      float ihwidth;
      float frhwidth = std::modf(width / 2, &ihwidth);
      const int ihw = ihwidth;

      for (int i = -ihw; i < ihw; i++) {
        if (position + i < static_cast<int>(stripe.size()) && position + i > 0) {
          stripe[position + i].color += render_color.to_rgb();
        }
      }

      // Brightness interpolation for the two pixels at the end
      render_color.v *= frhwidth;
      if (position - ihw < static_cast<int>(stripe.size()) && position - ihw > 0)
        stripe[position - ihw - 1].color += render_color.to_rgb();
      if (position + ihw < static_cast<int>(stripe.size()) && position + ihw > 0)
        stripe[position + ihw].color += render_color.to_rgb();
    }

    bool update() {
      if (age++ < 200)
        return true;

      width += 0.1;
      return (dim() > 1.f/255);
    }

    Melting(unsigned int position, float hue)
      : width(INITIAL_WIDTH), position(position), color(HSVColor{hue, 1, 1}), age(0) {
    }

  private:
    const float INITIAL_WIDTH = 30;
    float width;
    int position;
    HSVColor color;
    unsigned int age;

    float dim() const {
      if (age < 200)
        return (age * age) / 40000.f;

      const auto d = static_cast<float>(INITIAL_WIDTH) / width;
      return d * d * d;
    }
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

  std::list<std::shared_ptr<Sprite>> sprites;

  // PRNG for inserting new sprites.
  std::random_device r;
  std::default_random_engine e(r());
  std::uniform_int_distribution<int> pos_dist(0, STR_LEN - 1);
  std::uniform_int_distribution<unsigned char> col_dist(0, 255);
  std::uniform_real_distribution<float> vel_dist(-1, 1);
  std::uniform_real_distribution<float> hue_dist(0, 360);

  for (;;) { // Frame loop
    // Set up clock so we can sleep at the end of the frame
    auto start = std::chrono::steady_clock::now();

    std::vector<Pixel> stripe(STR_LEN); // Frame buffer

    // Insert new sprites
    static unsigned FC = 0;
    FC++;

    if (FC % 32 == 0)
      sprites.push_back(std::shared_ptr<Sprite>(
            new Pixel_Sprite(pos_dist(e),
                RGBColor{col_dist(e), col_dist(e), col_dist(e)},
                vel_dist(e))));

    if (FC % 16 == 0)
      sprites.push_back(std::shared_ptr<Sprite>(
            new Melting(pos_dist(e), hue_dist(e))));

    // Render all sprites
    for (auto& sprite : sprites)
      sprite->render(stripe);

    // Update all sprites and remove the ones that died
    for (auto spr_it = sprites.begin(); spr_it != sprites.end(); ) {
      if ((*spr_it)->update())
        spr_it++;
      else
        spr_it = sprites.erase(spr_it);
    }

    sender.send(serialize(stripe));
    std::cout << "Rendering " << sprites.size() << " sprites took " <<
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
          ).count()
        << "ms." << std::endl;
    std::this_thread::sleep_until(start + std::chrono::milliseconds(80));
  }
}
