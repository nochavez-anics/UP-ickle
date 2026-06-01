#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <cmath>
#include <algorithm>
#include <optional>
#include <string>
#include <cstdint>

using namespace std;

// ===================== BALL CLASS =====================
class Ball {
public:
    sf::CircleShape           shape;
    std::optional<sf::Sprite> sprite;
    sf::CircleShape           shadow;
    sf::Texture               texture;

    float vx = 0.f, vy = 0.f;
    float z  = 0.f, vz = 0.f;
    float spin      = 0.f;
    float rotation  = 0.f;
    float bouncePhase = 0.f;
    bool  bounceActive = false;
    bool  inPlay = false;

    float vertSpinAngle = 0.f;
    float vertSpinSpeed = 0.f;

    int   groundBounceCount = 0;
    const float bounceDampen = 0.52f;
    const float gravityZ     = 600.f;

    Ball() : shape(6.f), shadow(9.f) {
        shape.setFillColor(sf::Color::Transparent);
        shape.setOrigin({6.f, 6.f});
        shadow.setFillColor(sf::Color(0, 0, 0, 160));
        shadow.setOrigin({9.f, 4.5f});
    }

    bool loadTexture(const std::string& path) {
        if (!texture.loadFromFile(path)) return false;
        sprite.emplace(texture);
        sprite->setOrigin(sf::Vector2f(
            texture.getSize().x / 2.f,
            texture.getSize().y / 2.f));
        return true;
    }

    void setPosition(sf::Vector2f pos) { shape.setPosition(pos); }
    sf::Vector2f getPosition() const   { return shape.getPosition(); }
    sf::FloatRect getBounds() const    { return shape.getGlobalBounds(); }

    void reset(sf::Vector2f pos) {
        setPosition(pos);
        vx = vy = z = vz = spin = rotation = bouncePhase = 0.f;
        vertSpinAngle = vertSpinSpeed = 0.f;
        groundBounceCount = 0;
        bounceActive = false;
        inPlay = false;
        if (sprite) sprite->setRotation(sf::degrees(0.f));
    }

    void resetSpin() {
        spin = rotation = 0.f;
        vertSpinSpeed = vertSpinAngle = 0.f;
        if (sprite) sprite->setRotation(sf::degrees(0.f));
    }

    void updatePhysics(float dt) {
        vz -= gravityZ * dt;
        z  += vz * dt;

        if (z <= 0.f && vz < 0.f) {
            z = 0.f;
            groundBounceCount++;
            float damp = bounceDampen * (1.f - groundBounceCount * 0.12f);
            damp = max(damp, 0.f);
            vz = -vz * damp;
            if (vz < 30.f) { vz = 0.f; z = 0.f; }
        }

        if (vertSpinSpeed != 0.f) {
            vertSpinAngle += vertSpinSpeed * dt;
            vertSpinSpeed -= vertSpinSpeed * dt * 1.8f;
            if (abs(vertSpinSpeed) < 2.f) vertSpinSpeed = 0.f;
        } else if (vertSpinAngle != 0.f) {
            vertSpinAngle -= vertSpinAngle * dt * 4.f;
            if (abs(vertSpinAngle) < 0.5f) vertSpinAngle = 0.f;
        }

        if (bounceActive) {
            bouncePhase += dt * 8.f;
            if (bouncePhase >= 3.14159265f) {
                bouncePhase  = 3.14159265f;
                bounceActive = false;
            }
        }

        vx = clamp(vx, -550.f, 550.f);
        vy = clamp(vy, -600.f, 600.f);
    }

    void updateSprite(float dt) {
        float heightRatio = clamp(z / 120.f, 0.f, 1.f);

        // Scale matches main-2: 0.52 base + small height bonus
        float scale = 0.52f + heightRatio * 0.12f;

        if (bounceActive)
            scale *= 1.f + 0.08f * sin(bouncePhase);

        float squishX = 1.f, squishY = 1.f;
        if (z < 5.f && vz < -60.f && inPlay) {
            float t = 1.f - (z / 5.f);
            squishX = 1.f + t * 0.12f;
            squishY = 1.f - t * 0.10f;
        }

        float spinRate = 0.f;
        if (vertSpinSpeed != 0.f || abs(vertSpinAngle) > 0.5f) {
            float kickDriven = vertSpinSpeed * 0.18f;
            float velDriven  = vy * 0.55f;
            spinRate = (abs(vertSpinSpeed) > 2.f) ? kickDriven : velDriven;
        } else {
            spinRate = (abs(vx) > abs(vy)) ? vx * 0.55f : vy * 0.35f;
        }

        rotation += spinRate * dt;
        if (rotation >  360.f) rotation -= 360.f;
        if (rotation < -360.f) rotation += 360.f;

        if (sprite) {
            sprite->setRotation(sf::degrees(rotation));
            sprite->setScale(sf::Vector2f(scale * squishX, scale * squishY));
            sprite->setPosition(sf::Vector2f(
                shape.getPosition().x,
                shape.getPosition().y - z * 0.62f));
        }

        float shadowBaseScale = clamp(1.f - heightRatio * 0.30f, 0.70f, 1.f);
        int   shadowAlpha = (int)(220.f - heightRatio * 80.f);
        shadowAlpha = clamp(shadowAlpha, 80, 220);
        shadow.setFillColor(sf::Color(0, 0, 0, shadowAlpha));
        shadow.setScale(sf::Vector2f(shadowBaseScale, shadowBaseScale * 0.42f));
        shadow.setPosition(sf::Vector2f(
            shape.getPosition().x,
            shape.getPosition().y + 8.f));
    }

    void draw(sf::RenderWindow& window) {
        if (inPlay) window.draw(shadow);
        if (inPlay && sprite) window.draw(*sprite);
    }
};

// ===================== PLAYER CLASS =====================
class Player {
public:
    sf::RectangleShape shape;

    float speed;
    bool  dashing   = false;
    float dashTimer = 0.f;
    float dashDirX  = 0.f, dashDirY = 0.f;
    const float dashSpeed    = 700.f;
    const float dashTime     = 0.15f;

    bool  swinging   = false;
    float swingTimer = 0.f;
    const float swingDuration = 0.25f;

    float hitCooldown = 0.f;
    const float hitCooldownMax = 0.3f;

    Player(float spd) : shape({30.f, 40.f}), speed(spd) {
        shape.setFillColor(sf::Color::Transparent);
        shape.setOrigin({15.f, 20.f});
    }

    void setPosition(sf::Vector2f pos) { shape.setPosition(pos); }
    sf::Vector2f getPosition() const   { return shape.getPosition(); }

    sf::FloatRect getHitbox() const {
        sf::FloatRect r = shape.getGlobalBounds();
        r.position.x -= 20.f; r.size.x += 40.f;
        r.position.y -= 20.f; r.size.y += 40.f;
        return r;
    }

    void move(sf::Vector2f delta) { shape.move(delta); }

    void startDash(float dx, float dy) {
        dashing   = true;
        dashTimer = dashTime;
        dashDirX  = dx;
        dashDirY  = dy;
        if (dx == 0.f && dy == 0.f) dashDirY = 1.f;
    }

    void startSwing() {
        swinging   = true;
        swingTimer = swingDuration;
    }

    void update(float dt) {
        if (hitCooldown > 0.f) hitCooldown -= dt;
        if (swinging) { swingTimer -= dt; if (swingTimer <= 0.f) swinging = false; }
        if (dashing)  { dashTimer  -= dt; if (dashTimer  <= 0.f) dashing  = false; }
    }
};

// ===================== GAME CLASS =====================
class Game {
public:
    sf::RenderWindow window;

    sf::Texture               backgroundTexture;
    std::optional<sf::Sprite> backgroundSprite;
    sf::Texture               bgCourtTexture;
    std::optional<sf::Sprite> bgCourtSprite;
    sf::Texture               winnerBgTexture;
    std::optional<sf::Sprite> winnerBgSprite;

    // ── Court geometry ───────────────────────────────────────────────────────
    float bgW = 0.f, bgH = 0.f;
    const float courtLeft       = 495.f;
    const float courtRight      = 880.f;
    const float courtCenter     = 690.f;
    const float netY            = 375.f;
    const float courtTopEdge    = 90.f;
    const float courtBottomEdge = 665.f;
    const float fieldLeft       = courtLeft  - 100.f;   // 395
    const float fieldRight      = courtRight + 100.f;   // 980
    const float deadZoneTop     = courtTopEdge    - 20.f; // 70
    const float deadZoneBottom  = courtBottomEdge + 20.f; // 685
    const float outsideLeft     = 493.f;
    const float outsideRight    = 883.f;

    sf::RectangleShape court;
    sf::RectangleShape netLine;
    sf::RectangleShape midLine;

    Player p1, p2;
    Ball   ball;

    // ── Shared animation enum ────────────────────────────────────────────────
    enum class Anim { Idle, WalkSide, WalkFwd, Swing };

    // ── P1 Sprite Textures ───────────────────────────────────────────────────
    sf::Texture p1TexNormal, p1TexIdle;
    sf::Texture p1TexSwing1, p1TexSwing2;
    sf::Texture p1TexStep1,  p1TexStep2;
    sf::Texture p1TexFwd1,   p1TexFwd2;
    std::optional<sf::Sprite> p1Sprite;

    Anim  p1AnimState    = Anim::Idle;
    float p1IdleTimer    = 0.f;
    float p1IdleInterval = 0.55f;
    int   p1IdleFrame    = 0;
    float p1WalkTimer    = 0.f;
    float p1WalkInterval = 0.18f;
    int   p1WalkFrame    = 0;   // 0=step1, 1=normal, 2=step2
    bool  p1FacingLeft   = false;
    float p1FwdTimer     = 0.f;
    float p1FwdInterval  = 0.18f;
    int   p1FwdFrame     = 0;
    float p1SwingAnimTimer  = 0.f;
    int   p1SwingAnimFrame  = 0;
    bool  p1SwingAnimDone   = true;
    const float p1SwingFrameDur = 0.10f;

    // ── P2 Sprite Textures ───────────────────────────────────────────────────
    sf::Texture p2TexNormal, p2TexIdle;
    sf::Texture p2TexSwing1;
    sf::Texture p2TexStep1,  p2TexStep2;
    sf::Texture p2TexFwd1,   p2TexFwd2;
    std::optional<sf::Sprite> p2Sprite;

    // ── Score / Sigh animation textures & sprites ────────────────────────────
    sf::Texture p1ScoreTex1, p1ScoreTex2, p1SighTex;
    sf::Texture p2ScoreTex,  p2ScoreTex2, p2SighTex;
    std::optional<sf::Sprite> p1ScoreSprite1, p1ScoreSprite2, p1SighSprite;
    std::optional<sf::Sprite> p2ScoreSprite,  p2ScoreSprite2, p2SighSprite;

    // ── Score animation state ────────────────────────────────────────────────
    enum class ScoreState { None, P1Scored, P2Scored };
    ScoreState scoreState      = ScoreState::None;
    float      scoreStateTimer = 0.f;
    const float scoreStateDuration = 2.0f;

    float p1ScoreFrameTimer = 0.f;
    const float p1ScoreFrameDur = 0.35f;
    int   p1ScoreFrame = 0;

    float p2ScoreFrameTimer = 0.f;
    const float p2ScoreFrameDur = 0.35f;
    int   p2ScoreFrame = 0;

    // ── Score boxes ──────────────────────────────────────────────────────────
    sf::RectangleShape p1ScoreBox, p2ScoreBox;

    Anim  p2AnimState    = Anim::Idle;
    float p2IdleTimer    = 0.f;
    float p2IdleInterval = 0.55f;
    int   p2IdleFrame    = 0;
    float p2WalkTimer    = 0.f;
    float p2WalkInterval = 0.18f;
    int   p2WalkFrame    = 0;
    bool  p2FacingLeft   = false;
    float p2FwdTimer     = 0.f;
    float p2FwdInterval  = 0.18f;
    int   p2FwdFrame     = 0;
    float p2SwingAnimTimer  = 0.f;
    int   p2SwingAnimFrame  = 0;
    bool  p2SwingAnimDone   = true;
    const float p2SwingFrameDur = 0.18f;  // P2 only has 1 swing frame, hold longer

    // P2 swing circle
    sf::CircleShape p2SwingCircle;

    // ── Font & text ──────────────────────────────────────────────────────────
    sf::Font font;
    std::optional<sf::Text> score1Text, score2Text, serveText, winText, restartText;
    std::optional<sf::Text> p1PosText, p2PosText;

    int  score1 = 0, score2 = 0;
    const int winScore = 11;
    bool gameOver = false;

    // ── Win screen ───────────────────────────────────────────────────────────
    std::string winnerName;    // set to p1Name or p2Name on game over
    std::string p1Name = "P1";
    std::string p2Name = "P2";
    float flashTimer = 0.f;    // accumulates time for flashing
    float flashPhase = 0.f;    // sin-driven 0..1 for text flash alpha
    float burstAngle = 0.f;    // slowly rotating sunburst

    // ── Curve system ─────────────────────────────────────────────────────────
    float curveForce   = 0.f;
    float curveTargetY = 0.f;
    bool  curveActive  = false;
    bool  curvePassed  = false;
    bool  curvePending = false;

    bool ballOwner = true;
    bool serving   = true;

    const float playerSpeed = 250.f;

    // ---- Constructor ----
    Game()
        : window(sf::VideoMode({100, 100}), "Super Pickleball Adventure"),
          court({courtRight - courtLeft, courtBottomEdge - courtTopEdge}),
          netLine({courtRight - courtLeft, 10.f}),
          midLine({3.f, courtBottomEdge - courtTopEdge}),
          p1(250.f), p2(250.f),
          p2SwingCircle(35.f),
          p1ScoreBox({80.f, 80.f}),
          p2ScoreBox({80.f, 80.f})
    {
        p2SwingCircle.setFillColor(sf::Color(0, 200, 255, 120));
        p2SwingCircle.setOrigin({35.f, 35.f});

        p1ScoreBox.setFillColor(sf::Color(0, 0, 0, 200));
        p1ScoreBox.setOutlineThickness(3.f);
        p1ScoreBox.setOutlineColor(sf::Color::White);
        p1ScoreBox.setPosition({courtLeft - 140.f, netY - 90.f});

        p2ScoreBox.setFillColor(sf::Color(0, 0, 0, 200));
        p2ScoreBox.setOutlineThickness(3.f);
        p2ScoreBox.setOutlineColor(sf::Color::White);
        p2ScoreBox.setPosition({courtLeft - 140.f, netY + 10.f});
    }

    // ---- Helpers ----
    bool isNearCourtSideEdge(float x) const {
        const float edgeThreshold = 50.f;
        return x <= courtLeft + edgeThreshold || x >= courtRight - edgeThreshold;
    }

    float aimIntoCourt(float dirX, float fromX) const {
        if (fromX <= outsideLeft)  return  1.f;
        if (fromX >= outsideRight) return -1.f;
        return dirX;
    }

    // ---- Init ----
    bool init() {
        if (!backgroundTexture.loadFromFile("background.png")) return false;
        sf::Vector2u bgSize = backgroundTexture.getSize();
        bgW = static_cast<float>(bgSize.x);
        bgH = static_cast<float>(bgSize.y);

        window.close();
        window.create(sf::VideoMode(bgSize), "Super Pickleball Adventure");
        window.setFramerateLimit(60);

        backgroundSprite.emplace(backgroundTexture);
        backgroundSprite->setPosition({0.f, 0.f});

        if (!bgCourtTexture.loadFromFile("BG.png")) return false;
        bgCourtSprite.emplace(bgCourtTexture);
        bgCourtSprite->setScale(sf::Vector2f(
            bgW / bgCourtTexture.getSize().x,
            bgH / bgCourtTexture.getSize().y));
        bgCourtSprite->setPosition({0.f, 0.f});

        // Winner background — optional, gracefully absent
        if (winnerBgTexture.loadFromFile("winnerbg.png")) {
            winnerBgSprite.emplace(winnerBgTexture);
            winnerBgSprite->setScale(sf::Vector2f(
                bgW / winnerBgTexture.getSize().x,
                bgH / winnerBgTexture.getSize().y));
            winnerBgSprite->setPosition({0.f, 0.f});
        }

        court.setFillColor(sf::Color::Transparent);
        court.setOutlineColor(sf::Color::Transparent);
        court.setPosition({courtLeft, courtTopEdge});
        netLine.setFillColor(sf::Color::Transparent);
        netLine.setPosition({courtLeft, netY - 5.f});
        midLine.setFillColor(sf::Color::Transparent);
        midLine.setPosition({courtCenter - 1.5f, courtTopEdge});

        p1.setPosition({courtLeft + 295.f, courtBottomEdge});
        p2.setPosition({courtLeft + 100.f, courtTopEdge});

        if (!ball.loadTexture("ball.png")) return false;
        ball.setPosition({courtLeft + 295.f, courtBottomEdge - 35.f});

        // P1 textures
        if (!p1TexNormal.loadFromFile("P1.png"))              return false;
        if (!p1TexIdle.loadFromFile("P1idle.png"))            return false;
        if (!p1TexSwing1.loadFromFile("player1swing1.png"))   return false;
        if (!p1TexSwing2.loadFromFile("player1swing2.png"))   return false;
        if (!p1TexStep1.loadFromFile("p1step1.png"))          return false;
        if (!p1TexStep2.loadFromFile("p1step2.png"))          return false;
        if (!p1TexFwd1.loadFromFile("forward1.png"))          return false;
        if (!p1TexFwd2.loadFromFile("forward2.png"))          return false;

        p1Sprite.emplace(p1TexNormal);
        p1Sprite->setOrigin(sf::Vector2f(
            p1TexNormal.getSize().x / 2.f,
            p1TexNormal.getSize().y / 2.f));

        // P2 textures
        if (!p2TexNormal.loadFromFile("P2.png"))              return false;
        if (!p2TexIdle.loadFromFile("P2idle.png"))            return false;
        if (!p2TexSwing1.loadFromFile("player2swing1.png"))   return false;
        if (!p2TexStep1.loadFromFile("p2step1.png"))          return false;
        if (!p2TexStep2.loadFromFile("p2step2.png"))          return false;
        if (!p2TexFwd1.loadFromFile("p2forward1.png"))        return false;
        if (!p2TexFwd2.loadFromFile("p2forward2.png"))        return false;

        p2Sprite.emplace(p2TexNormal);
        p2Sprite->setOrigin(sf::Vector2f(
            p2TexNormal.getSize().x / 2.f,
            p2TexNormal.getSize().y / 2.f));

        // Score / Sigh animation textures
        if (!p1ScoreTex1.loadFromFile("p1score1.png")) return false;
        if (!p1ScoreTex2.loadFromFile("p1score2.png")) return false;
        if (!p1SighTex.loadFromFile("p1sigh.png"))     return false;
        if (!p2ScoreTex.loadFromFile("p2score.png"))   return false;
        if (!p2ScoreTex2.loadFromFile("P2.png"))       return false;
        if (!p2SighTex.loadFromFile("p2sigh.png"))     return false;

        p1ScoreSprite1.emplace(p1ScoreTex1);
        p1ScoreSprite2.emplace(p1ScoreTex2);
        p1SighSprite.emplace(p1SighTex);
        p2ScoreSprite.emplace(p2ScoreTex);
        p2ScoreSprite2.emplace(p2ScoreTex2);
        p2SighSprite.emplace(p2SighTex);

        // Font — pixel font for cohesive retro aesthetic across all screens
        // Expects PressStart2P-Regular.ttf next to the executable.
        // Falls back to a system font so the game still runs without it.
        if (!font.openFromFile("PressStart2P-Regular.ttf")) {
            // macOS fallback
            if (!font.openFromFile("/System/Library/Fonts/Helvetica.ttc")) return false;
        }

        score1Text.emplace(font, "0", 36);
        score1Text->setFillColor(sf::Color::White);
        score1Text->setStyle(sf::Text::Bold);
        score1Text->setOutlineColor(sf::Color::Black);
        score1Text->setOutlineThickness(2.f);
        score1Text->setPosition({p1ScoreBox.getPosition().x + 24.f,
                                  p1ScoreBox.getPosition().y + 10.f});

        score2Text.emplace(font, "0", 36);
        score2Text->setFillColor(sf::Color::White);
        score2Text->setStyle(sf::Text::Bold);
        score2Text->setOutlineColor(sf::Color::Black);
        score2Text->setOutlineThickness(2.f);
        score2Text->setPosition({p2ScoreBox.getPosition().x + 24.f,
                                  p2ScoreBox.getPosition().y + 10.f});

        serveText.emplace(font, "X to Serve (" + p1Name + ")", 12);
        serveText->setFillColor(sf::Color::Yellow);
        serveText->setOutlineColor(sf::Color(0, 0, 0, 180));
        serveText->setOutlineThickness(2.f);
        serveText->setPosition({courtLeft + 80.f, courtBottomEdge + 20.f});

        winText.emplace(font, "", 24);
        winText->setFillColor(sf::Color::Yellow);
        winText->setPosition({courtLeft + 20.f, netY - 40.f});

        restartText.emplace(font, "", 12);
        restartText->setFillColor(sf::Color::White);
        restartText->setPosition({courtLeft + 20.f, netY + 20.f});

        p1PosText.emplace(font, "P1 (0, 0)", 10);
        p1PosText->setFillColor(sf::Color::Yellow);
        p1PosText->setPosition({courtLeft - 80.f, courtBottomEdge - 40.f});

        p2PosText.emplace(font, "P2 (0, 0)", 10);
        p2PosText->setFillColor(sf::Color::Cyan);
        p2PosText->setPosition({courtLeft - 80.f, courtTopEdge + 40.f});

        return true;
    }

    // ---- Name Entry ----
    void showNameEntry() {
        // Reuse the main window — it's already open after init()
        const float W = bgW, H = bgH;
        const float cx = W * 0.5f, cy = H * 0.5f;

        // Which player we're currently naming (0 = P1, 1 = P2)
        int stage = 0;
        std::string drafts[2] = {"", ""};
        const std::string defaults[2] = {"P1", "P2"};
        const std::string prompts[2]  = {
            "ENTER PLAYER 1 NAME",
            "ENTER PLAYER 2 NAME"
        };
        // Gold for P1, orange-red for P2 — matching win screen colours
        const sf::Color colours[2] = {
            sf::Color(100, 220, 255),   // P1 cyan
            sf::Color(255, 140, 100)    // P2 orange
        };
        // Box border colours per player
        const sf::Color borderColours[2] = {
            sf::Color(255, 215,  0),    // gold
            sf::Color(255, 215,  0)     // gold
        };

        // Cursor blink
        float cursorTimer = 0.f;
        bool  cursorOn    = true;
        // Cursor rectangle — pixel-art style solid block
        sf::RectangleShape cursorBlock({4.f, 26.f});
        cursorBlock.setFillColor(sf::Color(255, 215, 0));

        sf::Clock clock;

        auto submit = [&]() {
            if (drafts[stage].empty()) drafts[stage] = defaults[stage];
            if (drafts[stage].size() > 12) drafts[stage].resize(12);
            stage++;
        };

        while (window.isOpen() && stage < 2) {
            float dt = clock.restart().asSeconds();
            cursorTimer += dt;
            if (cursorTimer >= 0.5f) { cursorTimer = 0.f; cursorOn = !cursorOn; }

            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) { window.close(); return; }

                if (const auto* te = event->getIf<sf::Event::TextEntered>()) {
                    uint32_t c = te->unicode;
                    if (c == 8 && !drafts[stage].empty())   // backspace
                        drafts[stage].pop_back();
                    else if (c == 13)                        // Enter
                        submit();
                    else if (c >= 32 && c < 127 && drafts[stage].size() < 12)
                        drafts[stage] += static_cast<char>(c);
                }
                if (const auto* ke = event->getIf<sf::Event::KeyPressed>()) {
                    if (ke->code == sf::Keyboard::Key::Enter) submit();
                }
            }

            window.clear();
            // Draw court background
            if (bgCourtSprite)         window.draw(*bgCourtSprite);
            else if (backgroundSprite) window.draw(*backgroundSprite);

            // Semi-transparent dark overlay so UI pops
            sf::RectangleShape overlay({W, H});
            overlay.setFillColor(sf::Color(0, 0, 10, 170));
            window.draw(overlay);

            if (font.getInfo().family != "") {
                // ── "ENTER PLAYER NAME" title — pixel gold with dark outline ──
                sf::Text title(font, prompts[stage], 28);
                title.setFillColor(sf::Color(255, 215, 0));
                title.setOutlineColor(sf::Color(60, 30, 0, 220));
                title.setOutlineThickness(3.f);
                {
                    sf::FloatRect b = title.getLocalBounds();
                    title.setOrigin({b.position.x + b.size.x/2.f,
                                     b.position.y + b.size.y/2.f});
                    title.setPosition({cx, cy - 140.f});
                }
                window.draw(title);

                // ── Pixel-art input panel ──────────────────────────────────
                const float boxW = 480.f, boxH = 64.f;
                // Outer border (4 px pixel-style double border)
                sf::RectangleShape outerBox({boxW + 8.f, boxH + 8.f});
                outerBox.setFillColor(sf::Color(255, 215, 0));          // gold fill acts as border
                outerBox.setOrigin({(boxW + 8.f)/2.f, (boxH + 8.f)/2.f});
                outerBox.setPosition({cx, cy + 10.f});
                window.draw(outerBox);

                // Inner dark box
                sf::RectangleShape innerBox({boxW, boxH});
                innerBox.setFillColor(sf::Color(10, 10, 40, 245));
                innerBox.setOrigin({boxW/2.f, boxH/2.f});
                innerBox.setPosition({cx, cy + 10.f});
                window.draw(innerBox);

                // Typed text (centred in box)
                sf::Text inputText(font, drafts[stage], 24);
                inputText.setFillColor(colours[stage]);
                inputText.setOutlineColor(sf::Color(0, 0, 0, 160));
                inputText.setOutlineThickness(1.5f);
                {
                    sf::FloatRect b = inputText.getLocalBounds();
                    // Left-align inside box — start at box left edge + padding
                    float textX = cx - boxW/2.f + 18.f;
                    float textY = cy + 10.f - b.size.y/2.f - b.position.y;
                    inputText.setPosition({textX, textY});

                    // Draw blinking cursor block after last character
                    if (cursorOn) {
                        cursorBlock.setPosition({textX + b.size.x + 6.f, textY + 2.f});
                        window.draw(cursorBlock);
                    }
                }
                window.draw(inputText);

                // ── "PRESS ENTER TO CONFIRM" hint ─────────────────────────
                sf::Text hint(font, "PRESS ENTER TO CONFIRM", 10);
                hint.setFillColor(sf::Color(180, 180, 200, 190));
                {
                    sf::FloatRect b = hint.getLocalBounds();
                    hint.setOrigin({b.position.x + b.size.x/2.f,
                                    b.position.y + b.size.y/2.f});
                    hint.setPosition({cx, cy + 75.f});
                }
                window.draw(hint);

                // ── Player badge strip (shows which player is being named) ─
                {
                    // Small coloured pill label
                    const sf::Color badgeCol = colours[stage];
                    sf::RectangleShape badge({160.f, 32.f});
                    badge.setFillColor(sf::Color(badgeCol.r, badgeCol.g, badgeCol.b, 50));
                    badge.setOutlineColor(badgeCol);
                    badge.setOutlineThickness(2.f);
                    badge.setOrigin({80.f, 16.f});
                    badge.setPosition({cx, cy - 60.f});
                    window.draw(badge);

                    sf::Text badgeTxt(font,
                        stage == 0 ? "PLAYER 1" : "PLAYER 2", 12);
                    badgeTxt.setFillColor(badgeCol);
                    sf::FloatRect bb = badgeTxt.getLocalBounds();
                    badgeTxt.setOrigin({bb.position.x + bb.size.x/2.f,
                                        bb.position.y + bb.size.y/2.f});
                    badgeTxt.setPosition({cx, cy - 60.f});
                    window.draw(badgeTxt);
                }

                // ── Already-confirmed P1 name shown while entering P2 ─────
                if (stage == 1 && !drafts[0].empty()) {
                    sf::Text done(font, "P1: " + drafts[0], 12);
                    done.setFillColor(sf::Color(100, 220, 255, 210));
                    done.setOutlineColor(sf::Color(0,0,0,140));
                    done.setOutlineThickness(1.5f);
                    {
                        sf::FloatRect b = done.getLocalBounds();
                        done.setOrigin({b.position.x + b.size.x/2.f,
                                        b.position.y + b.size.y/2.f});
                        done.setPosition({cx, cy + 118.f});
                    }
                    window.draw(done);
                }
            }

            window.display();
        }

        if (!window.isOpen()) return;

        // Commit names
        p1Name = drafts[0].empty() ? defaults[0] : drafts[0];
        p2Name = drafts[1].empty() ? defaults[1] : drafts[1];

        // Score texts are numeric — no name prefix needed
        if (serveText)  serveText->setString("X to Serve (" + p1Name + ")");
        if (p1PosText)  p1PosText->setString(p1Name + " (0, 0)");
        if (p2PosText)  p2PosText->setString(p2Name + " (0, 0)");
    }

    // ---- Splash ----
    void showSplash() {
        sf::RenderWindow splash(
            sf::VideoMode({static_cast<unsigned>(bgW), static_cast<unsigned>(bgH)}),
            "Super Pickleball Adventure - Press X to Start");
        splash.setFramerateLimit(60);
        while (splash.isOpen()) {
            while (const auto event = splash.pollEvent()) {
                if (event->is<sf::Event::Closed>()) splash.close();
                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                    if (key->code == sf::Keyboard::Key::X) splash.close();
            }
            splash.clear();
            if (backgroundSprite) splash.draw(*backgroundSprite);
            splash.display();
        }
    }

    // ---- Hit functions ----
    void hitToP2Field(float dirX, bool dashing) {
        dirX = aimIntoCourt(dirX, ball.getPosition().x);
        float initialSide = 0.f;
        bool  edgeHit     = false;
        float bx = ball.getPosition().x;

        if      (bx <= outsideLeft)  initialSide =  120.f;
        else if (bx >= outsideRight) initialSide = -120.f;
        else if (isNearCourtSideEdge(bx) && dirX != 0.f)
            { initialSide = dirX * 45.f; edgeHit = true; }
        else
            initialSide = dirX * 45.f;

        ball.vx = initialSide; ball.vy = -420.f;
        ball.z  = 22.f;        ball.vz = 240.f;
        ball.groundBounceCount = 0;
        ball.bounceActive = false; ball.bouncePhase = 0.f;

        if (dashing) {
            ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
            ball.spin = dirX * 900.f;
        } else {
            float move = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) move = -1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) move =  1.f;
            if (dirX != 0.f) {
                ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
                ball.spin = dirX * 300.f + move * 160.f;
            } else {
                ball.spin = 0.f; ball.vertSpinAngle = 0.f;
                ball.vertSpinSpeed = -720.f;
            }
        }

        curveTargetY = courtTopEdge + 175.f;
        curveForce   = (dirX != 0.f) ? dirX * (dashing ? 220.f : 125.f) : 0.f;
        curvePending = edgeHit;
        curveActive  = !curvePending && (dirX != 0.f);
        curvePassed  = false;
        ball.inPlay  = true;
    }

    void hitToP1Field(float dirX, bool dashing) {
        dirX = aimIntoCourt(dirX, ball.getPosition().x);
        float initialSide = 0.f;
        bool  edgeHit     = false;
        float bx = ball.getPosition().x;

        if      (bx <= outsideLeft)  initialSide =  120.f;
        else if (bx >= outsideRight) initialSide = -120.f;
        else if (isNearCourtSideEdge(bx) && dirX != 0.f)
            { initialSide = dirX * 45.f; edgeHit = true; }
        else
            initialSide = dirX * 45.f;

        ball.vx = initialSide; ball.vy = 420.f;
        ball.z  = 22.f;        ball.vz = 240.f;
        ball.groundBounceCount = 0;
        ball.bounceActive = false; ball.bouncePhase = 0.f;

        if (dashing) {
            ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
            ball.spin = dirX * 900.f;
        } else {
            float move = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  move = -1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) move =  1.f;
            if (dirX != 0.f) {
                ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
                ball.spin = dirX * 300.f + move * 160.f;
            } else {
                ball.spin = 0.f; ball.vertSpinAngle = 0.f;
                ball.vertSpinSpeed = 720.f;
            }
        }

        curveTargetY = courtTopEdge + 425.f;
        curveForce   = (dirX != 0.f) ? dirX * (dashing ? 220.f : 125.f) : 0.f;
        curvePending = edgeHit;
        curveActive  = !curvePending && (dirX != 0.f);
        curvePassed  = false;
        ball.inPlay  = true;
    }

    // ---- Reset ----
    void resetAfterPoint() {
        p1.setPosition({courtLeft + 295.f, courtBottomEdge});
        p2.setPosition({courtLeft + 100.f, courtTopEdge});
        ball.reset({courtLeft + 295.f, courtBottomEdge - 35.f});
        curveActive = curvePassed = curvePending = false;
    }

    void fullReset() {
        score1 = 0; score2 = 0;
        score1Text->setString("0");
        score2Text->setString("0");
        gameOver    = false;
        winnerName  = "";
        flashTimer  = 0.f;
        burstAngle  = 0.f;
        ballOwner = true;
        serving   = true;
        winText->setString("");
        restartText->setString("");
        serveText->setString("X to Serve (" + p1Name + ")");
        serveText->setPosition({courtLeft + 80.f, courtBottomEdge + 20.f});
        scoreState = ScoreState::None;
        scoreStateTimer = 0.f;
        p1ScoreFrame = p2ScoreFrame = 0;
        p1ScoreFrameTimer = p2ScoreFrameTimer = 0.f;
        resetAfterPoint();
    }

    // ---- Handle events ----
    void handleEvents() {
        while (const optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (gameOver && key->code == sf::Keyboard::Key::R) { fullReset(); return; }
                if (gameOver) return;

                // P1 SERVE
                if (key->code == sf::Keyboard::Key::X && !ball.inPlay && ballOwner) {
                    float dirX = -1.f;  // Force left direction
                    ball.vx = -520.f;   // Strong leftward force
                    ball.vy = -200.f;   // Minimal upward velocity to reach opponent's court
                    ball.z  = 22.f;
                    ball.vz = 240.f;
                    ball.groundBounceCount = 0;
                    ball.bounceActive = false; ball.bouncePhase = 0.f;
                    ball.spin = dirX * 600.f;
                    ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
                    curveActive = curvePassed = curvePending = false;
                    p1.startSwing();
                    p1SwingAnimFrame = 0; p1SwingAnimTimer = 0.f;
                    p1SwingAnimDone  = false; p1AnimState = Anim::Swing;
                    ball.inPlay = true; serving = false;
                    serveText->setString("");
                }

                // P2 SERVE
                if (key->code == sf::Keyboard::Key::Period && !ball.inPlay && !ballOwner) {
                    float dirX = 1.f;   // Force right direction
                    ball.vx = 520.f;    // Strong rightward force
                    ball.vy = 200.f;    // Minimal downward velocity to reach opponent's court
                    ball.z  = 22.f;
                    ball.vz = 240.f;
                    ball.groundBounceCount = 0;
                    ball.bounceActive = false; ball.bouncePhase = 0.f;
                    ball.spin = dirX * 600.f;
                    ball.vertSpinSpeed = 0.f; ball.vertSpinAngle = 0.f;
                    curveActive = curvePassed = curvePending = false;
                    p2.startSwing();
                    p2SwingAnimFrame = 0; p2SwingAnimTimer = 0.f;
                    p2SwingAnimDone  = false; p2AnimState = Anim::Swing;
                    ball.inPlay = true; serving = false;
                    serveText->setString("");
                }

                // P1 DASH
                if (key->code == sf::Keyboard::Key::Q && !p1.dashing && ball.inPlay) {
                    float dx = 0.f, dy = 0.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dx = -1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dx =  1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) dy = -1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) dy =  1.f;
                    if (dx == 0.f && dy == 0.f) dy = -1.f;
                    p1.startDash(dx, dy);
                }

                // P2 DASH
                if (key->code == sf::Keyboard::Key::RShift && !p2.dashing && ball.inPlay) {
                    float dx = 0.f, dy = 0.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dx = -1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dx =  1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    dy = -1.f;
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  dy =  1.f;
                    if (dx == 0.f && dy == 0.f) dy = 1.f;
                    p2.startDash(dx, dy);
                }

                // P1 SWING
                if (key->code == sf::Keyboard::Key::Z && !p1.swinging) {
                    p1.startSwing();
                    p1SwingAnimFrame = 0; p1SwingAnimTimer = 0.f;
                    p1SwingAnimDone  = false; p1AnimState = Anim::Swing;
                }

                // P2 SWING
                if (key->code == sf::Keyboard::Key::Slash && !p2.swinging) {
                    p2.startSwing();
                    p2SwingAnimFrame = 0; p2SwingAnimTimer = 0.f;
                    p2SwingAnimDone  = false; p2AnimState = Anim::Swing;
                }
            }
        }
    }

    // ---- P1 animation ----
    void updateP1Animation(float dt) {
        bool p1MovingLeft  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
        bool p1MovingRight = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
        bool p1MovingUp    = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
        bool p1MovingDown  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
        bool p1MovingSide  = p1MovingLeft || p1MovingRight;
        bool p1MovingFwd   = p1MovingUp   || p1MovingDown;

        if (p1.dashing) {
            p1MovingSide = (p1.dashDirX != 0.f);
            p1MovingFwd  = (p1.dashDirY != 0.f);
            if (p1.dashDirX < 0.f) p1FacingLeft = true;
            if (p1.dashDirX > 0.f) p1FacingLeft = false;
        } else {
            if (p1MovingLeft)  p1FacingLeft = true;
            if (p1MovingRight) p1FacingLeft = false;
        }

        if (p1AnimState == Anim::Swing) {
            if (!p1SwingAnimDone) {
                p1SwingAnimTimer += dt;
                if (p1SwingAnimTimer >= p1SwingFrameDur) {
                    p1SwingAnimTimer -= p1SwingFrameDur;
                    p1SwingAnimFrame++;
                    if (p1SwingAnimFrame >= 2) {
                        p1SwingAnimDone = true;
                        p1SwingAnimFrame = 1; // hold last frame
                        if      (p1MovingSide) p1AnimState = Anim::WalkSide;
                        else if (p1MovingFwd)  p1AnimState = Anim::WalkFwd;
                        else                   p1AnimState = Anim::Idle;
                    }
                }
            }
        } else if (p1MovingSide) {
            p1AnimState = Anim::WalkSide;
            p1FwdTimer  = 0.f;
            p1WalkTimer += dt;
            if (p1WalkTimer >= p1WalkInterval) {
                p1WalkTimer -= p1WalkInterval;
                p1WalkFrame  = (p1WalkFrame + 1) % 3; // 3-frame cycle
            }
        } else if (p1MovingFwd) {
            p1AnimState = Anim::WalkFwd;
            p1WalkTimer = 0.f;
            p1FwdTimer += dt;
            if (p1FwdTimer >= p1FwdInterval) {
                p1FwdTimer -= p1FwdInterval;
                p1FwdFrame  = 1 - p1FwdFrame;
            }
        } else {
            p1AnimState = Anim::Idle;
            p1WalkTimer = 0.f;
            p1FwdTimer  = 0.f;
            p1WalkFrame = 0; // reset so walk starts from step1
            p1IdleTimer += dt;
            if (p1IdleTimer >= p1IdleInterval) {
                p1IdleTimer -= p1IdleInterval;
                p1IdleFrame  = 1 - p1IdleFrame;
            }
        }

        switch (p1AnimState) {
        case Anim::Swing:
            p1Sprite->setTexture(p1SwingAnimFrame == 0 ? p1TexSwing1 : p1TexSwing2);
            break;
        case Anim::WalkSide: {
            // 3-frame: 0=step1, 1=normal, 2=step2 (reversed when facing left)
            int displayFrame = p1FacingLeft ? (2 - p1WalkFrame) : p1WalkFrame;
            if      (displayFrame == 0) p1Sprite->setTexture(p1TexStep1);
            else if (displayFrame == 1) p1Sprite->setTexture(p1TexNormal);
            else                        p1Sprite->setTexture(p1TexStep2);
            break;
        }
        case Anim::WalkFwd:
            p1Sprite->setTexture(p1FwdFrame == 0 ? p1TexFwd1 : p1TexFwd2);
            break;
        case Anim::Idle:
        default:
            p1Sprite->setTexture(p1IdleFrame == 0 ? p1TexNormal : p1TexIdle);
            break;
        }

        {
            auto texSize = p1Sprite->getTexture().getSize();
            float tw = static_cast<float>(texSize.x);
            float th = static_cast<float>(texSize.y);
            p1Sprite->setOrigin({tw / 2.f, th / 2.f});
            const float targetW = 72.f, targetH = 96.f;
            float baseScale = min(targetW / tw, targetH / th);
            bool mirror = p1FacingLeft;
            p1Sprite->setScale({ mirror ? -baseScale : baseScale, baseScale });
        }
        p1Sprite->setPosition(p1.getPosition());

        // ── P1 score/sigh sprite sync — recompute every frame so they
        //    always track the live p1 position and facing direction
        {
            const float targetW = 72.f, targetH = 96.f;
            bool mirror = p1FacingLeft;

            // P1 score sprites (shown when P1 scored)
            auto syncP1Score = [&](sf::Sprite& spr, const sf::Texture& tex, float overlayScale = 1.f) {
                auto sz = tex.getSize();
                float tw = (float)sz.x, th = (float)sz.y;
                float base = min(targetW / tw, targetH / th) * overlayScale;
                spr.setOrigin({tw / 2.f, th / 2.f});
                spr.setScale({ mirror ? -base : base, base });
                spr.setPosition(p1.getPosition());
            };
            if (p1ScoreSprite1 && p1ScoreSprite2) {
                syncP1Score(*p1ScoreSprite1, p1ScoreTex1);
                syncP1Score(*p1ScoreSprite2, p1ScoreTex2);
            }

            // P1 sigh sprite (shown when P2 scored)
            if (p1SighSprite) {
                auto sz = p1SighTex.getSize();
                float tw = (float)sz.x, th = (float)sz.y;
                float base = min(targetW / tw, targetH / th);
                p1SighSprite->setOrigin({tw / 2.f, th / 2.f});
                p1SighSprite->setScale({ mirror ? -base : base, base });
                p1SighSprite->setPosition(p1.getPosition());
            }
        }
    }

    // ---- P2 animation ----
    void updateP2Animation(float dt) {
        bool p2MovingLeft  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        bool p2MovingRight = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        bool p2MovingUp    = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
        bool p2MovingDown  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);
        bool p2MovingSide  = p2MovingLeft || p2MovingRight;
        bool p2MovingFwd   = p2MovingUp   || p2MovingDown;

        if (p2.dashing) {
            p2MovingSide = (p2.dashDirX != 0.f);
            p2MovingFwd  = (p2.dashDirY != 0.f);
            if (p2.dashDirX < 0.f) p2FacingLeft = true;
            if (p2.dashDirX > 0.f) p2FacingLeft = false;
        } else {
            if (p2MovingLeft)  p2FacingLeft = true;
            if (p2MovingRight) p2FacingLeft = false;
        }

        // P2 has only 1 swing frame — hold it for swingFrameDur then done
        if (p2AnimState == Anim::Swing) {
            if (!p2SwingAnimDone) {
                p2SwingAnimTimer += dt;
                if (p2SwingAnimTimer >= p2SwingFrameDur) {
                    p2SwingAnimDone  = true;
                    p2SwingAnimFrame = 0;
                    p2AnimState = p2MovingSide ? Anim::WalkSide
                                : p2MovingFwd  ? Anim::WalkFwd
                                :                Anim::Idle;
                }
            }
        } else if (p2MovingSide) {
            p2AnimState = Anim::WalkSide;
            p2FwdTimer  = 0.f;
            p2WalkTimer += dt;
            if (p2WalkTimer >= p2WalkInterval) {
                p2WalkTimer -= p2WalkInterval;
                p2WalkFrame  = (p2WalkFrame + 1) % 3;
            }
        } else if (p2MovingFwd) {
            p2AnimState = Anim::WalkFwd;
            p2WalkTimer = 0.f;
            p2FwdTimer += dt;
            if (p2FwdTimer >= p2FwdInterval) {
                p2FwdTimer -= p2FwdInterval;
                p2FwdFrame  = 1 - p2FwdFrame;
            }
        } else {
            p2AnimState = Anim::Idle;
            p2WalkTimer = 0.f;
            p2FwdTimer  = 0.f;
            p2WalkFrame = 0;
            p2IdleTimer += dt;
            if (p2IdleTimer >= p2IdleInterval) {
                p2IdleTimer -= p2IdleInterval;
                p2IdleFrame  = 1 - p2IdleFrame;
            }
        }

        switch (p2AnimState) {
        case Anim::Swing:
            p2Sprite->setTexture(p2TexSwing1);
            break;
        case Anim::WalkSide: {
            int displayFrame = p2FacingLeft ? (2 - p2WalkFrame) : p2WalkFrame;
            if      (displayFrame == 0) p2Sprite->setTexture(p2TexStep1);
            else if (displayFrame == 1) p2Sprite->setTexture(p2TexNormal);
            else                        p2Sprite->setTexture(p2TexStep2);
            break;
        }
        case Anim::WalkFwd:
            p2Sprite->setTexture(p2FwdFrame == 0 ? p2TexFwd1 : p2TexFwd2);
            break;
        case Anim::Idle:
        default:
            p2Sprite->setTexture(p2IdleFrame == 0 ? p2TexNormal : p2TexIdle);
            break;
        }

        {
            auto texSize = p2Sprite->getTexture().getSize();
            float tw = static_cast<float>(texSize.x);
            float th = static_cast<float>(texSize.y);
            p2Sprite->setOrigin({tw / 2.f, th / 2.f});
            const float targetW = 72.f, targetH = 96.f;
            float baseScale = min(targetW / tw, targetH / th);
            bool mirror = p2FacingLeft;
            p2Sprite->setScale({ mirror ? -baseScale : baseScale, baseScale });
        }
        p2Sprite->setPosition(p2.getPosition());

        // ── P2 score/sigh sprite sync — recompute every frame so they
        //    always track the live p2 position and facing direction
        {
            const float targetW = 72.f, targetH = 96.f;
            bool mirror = p2FacingLeft;

            // P2 score sprites (shown when P2 scored) — slightly enlarged like main-3
            auto syncP2Score = [&](sf::Sprite& spr, const sf::Texture& tex, float overlayScale) {
                auto sz = tex.getSize();
                float tw = (float)sz.x, th = (float)sz.y;
                float base = min(targetW / tw, targetH / th) * overlayScale;
                spr.setOrigin({tw / 2.f, th / 2.f});
                spr.setScale({ mirror ? -base : base, base });
                spr.setPosition(p2.getPosition());
            };
            if (p2ScoreSprite)  syncP2Score(*p2ScoreSprite,  p2ScoreTex,  1.30f);
            if (p2ScoreSprite2) syncP2Score(*p2ScoreSprite2, p2ScoreTex2, 1.15f);

            // P2 sigh sprite (shown when P1 scored)
            if (p2SighSprite) {
                auto sz = p2SighTex.getSize();
                float tw = (float)sz.x, th = (float)sz.y;
                float base = min(targetW / tw, targetH / th) * 1.30f;
                p2SighSprite->setOrigin({tw / 2.f, th / 2.f});
                p2SighSprite->setScale({ mirror ? -base : base, base });
                p2SighSprite->setPosition(p2.getPosition());
            }
        }
    }

    // ---- Update players ----
    void updatePlayers(float dt) {
        // P1 movement
        if (!p1.dashing) {
            float vx = 0.f, vy = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) vx = -playerSpeed;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) vx =  playerSpeed;
            if (!ballOwner || ball.inPlay) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) vy = -playerSpeed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) vy =  playerSpeed;
            }
            p1.move({vx * dt, vy * dt});
        } else {
            p1.move({p1.dashDirX * p1.dashSpeed * dt,
                     p1.dashDirY * p1.dashSpeed * dt});
        }

        // P2 movement
        if (!p2.dashing) {
            float vx = 0.f, vy = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  vx = -playerSpeed;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) vx =  playerSpeed;
            if (ballOwner || ball.inPlay) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))   vy = -playerSpeed;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) vy =  playerSpeed;
            }
            p2.move({vx * dt, vy * dt});
        } else {
            p2.move({p2.dashDirX * p2.dashSpeed * dt,
                     p2.dashDirY * p2.dashSpeed * dt});
        }

        // Clamp P1 — matches main-2 fixed Y values
        auto pos1 = p1.getPosition();
        if (!ball.inPlay && ballOwner)
            pos1.x = clamp(pos1.x, courtCenter, courtRight);
        else
            pos1.x = clamp(pos1.x, fieldLeft, fieldRight);
        pos1.y = clamp(pos1.y, 496.f, courtBottomEdge);
        p1.setPosition(pos1);

        // Clamp P2 — matches main-2 fixed Y values
        auto pos2 = p2.getPosition();
        if (!ball.inPlay && !ballOwner)
            pos2.x = clamp(pos2.x, courtLeft, courtCenter);
        else
            pos2.x = clamp(pos2.x, fieldLeft, fieldRight);
        pos2.y = clamp(pos2.y, courtTopEdge, 224.f);
        p2.setPosition(pos2);

        p1.update(dt);
        p2.update(dt);

        updateP1Animation(dt);
        updateP2Animation(dt);
    }

    // ---- Update ball ----
    void updateBall(float dt) {
        if (!ball.inPlay) {
            ball.z = ball.vz = 0.f;
            ball.groundBounceCount = 0;
            ball.resetSpin();
            if (ballOwner)
                ball.setPosition({p1.getPosition().x, p1.getPosition().y - 30.f});
            else
                ball.setPosition({p2.getPosition().x, p2.getPosition().y + 30.f});
            return;
        }

        ball.updatePhysics(dt);

        auto bpos = ball.getPosition();

        if (curvePending) {
            bool reached = (ball.vy < 0.f && bpos.y <= curveTargetY) ||
                           (ball.vy > 0.f && bpos.y >= curveTargetY);
            if (reached) { curvePending = false; curveActive = (curveForce != 0.f); }
        }

        if (curveActive) {
            bool reached = (ball.vy < 0.f && bpos.y <= curveTargetY) ||
                           (ball.vy > 0.f && bpos.y >= curveTargetY);
            if (!curvePassed && reached) {
                ball.vx += curveForce * dt * 30.f;
                ball.vx  = clamp(ball.vx, -320.f, 320.f);
                if (abs(ball.vx) >= abs(curveForce) * 0.75f) {
                    ball.vx       = curveForce;
                    curvePassed   = true;
                    curveActive   = false;
                    ball.bounceActive = true;
                    ball.bouncePhase  = -3.14159265f / 2.f;
                    if (abs(curveForce) >= 180.f) {
                        ball.vy *= 1.4f;
                        ball.vx *= 1.1f;
                        ball.vx  = clamp(ball.vx, -500.f, 500.f);
                        ball.spin *= 2.2f;
                        ball.bouncePhase = -3.14159265f;
                    }
                }
            }
        }

        if (bpos.x <= outsideLeft  && ball.vx < 0.f) ball.vx += 80.f * dt;
        if (bpos.x >= outsideRight && ball.vx > 0.f) ball.vx -= 80.f * dt;

        if (bpos.x <= fieldLeft) {
            ball.setPosition({fieldLeft + 1.f, bpos.y});
            ball.vx = abs(ball.vx) * 0.5f;
            bpos = ball.getPosition();
        }
        if (bpos.x >= fieldRight) {
            ball.setPosition({fieldRight - 1.f, bpos.y});
            ball.vx = -abs(ball.vx) * 0.5f;
            bpos = ball.getPosition();
        }

        float prevY  = bpos.y;
        float nextX = clamp(bpos.x + ball.vx * dt, fieldLeft, fieldRight);
        float nextY = bpos.y + ball.vy * dt;

        // Net collision — ball must clear the net (z > 20) to cross
        bool crossingNet = (prevY < netY && nextY >= netY) ||
                           (prevY > netY && nextY <= netY);
        if (crossingNet && ball.z < 20.f) {
            // Hit the net — reverse vertical velocity, damp, bounce back
            ball.vy *= -0.35f;
            ball.vx *= 0.6f;
            ball.vz  = max(ball.vz, 60.f);
            nextY    = prevY; // don't cross
        }

        ball.setPosition({nextX, nextY});
        bpos = ball.getPosition();

        // Scoring
        if (bpos.y < deadZoneTop) {
            score1++;
            score1Text->setString(to_string(score1));
            ball.inPlay = false; ballOwner = false; serving = true;
            curveActive = false; curvePending = false;
            serveText->setString(". to Serve (" + p2Name + ")");
            serveText->setPosition({courtLeft + 80.f, courtTopEdge - 50.f});
            // Trigger score animation: P1 scored
            scoreState = ScoreState::P1Scored;
            scoreStateTimer = scoreStateDuration;
            p1ScoreFrame = 0; p1ScoreFrameTimer = 0.f;
            resetAfterPoint();
            if (score1 >= winScore) {
                gameOver    = true;
                winnerName  = p1Name;
                flashTimer  = 0.f;
                burstAngle  = 0.f;
                winText->setString(p1Name + " Wins!");
                restartText->setString("Press R to play again");
                serveText->setString("");
            }
            return;
        }
        if (bpos.y > deadZoneBottom) {
            score2++;
            score2Text->setString(to_string(score2));
            ball.inPlay = false; ballOwner = true; serving = true;
            curveActive = false; curvePending = false;
            serveText->setString("X to Serve (" + p1Name + ")");
            serveText->setPosition({courtLeft + 80.f, courtBottomEdge + 20.f});
            // Trigger score animation: P2 scored
            scoreState = ScoreState::P2Scored;
            scoreStateTimer = scoreStateDuration;
            p2ScoreFrame = 0; p2ScoreFrameTimer = 0.f;
            resetAfterPoint();
            if (score2 >= winScore) {
                gameOver    = true;
                winnerName  = p2Name;
                flashTimer  = 0.f;
                burstAngle  = 0.f;
                winText->setString(p2Name + " Wins!");
                restartText->setString("Press R to play again");
                serveText->setString("");
            }
            return;
        }

        // Hit detection
        sf::FloatRect ballB = ball.getBounds();

        bool p1CanHit = (p1.swinging || p1.dashing) &&
                         p1.hitCooldown <= 0.f &&
                         ball.getPosition().y > netY + 25.f;
        if (p1CanHit && ballB.findIntersection(p1.getHitbox())) {
            float dirX = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dirX = -1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dirX =  1.f;
            hitToP2Field(dirX, p1.dashing);
            p1.swinging    = false;
            p1.hitCooldown = p1.hitCooldownMax;
            p1SwingAnimFrame = 0; p1SwingAnimTimer = 0.f;
            p1SwingAnimDone  = false; p1AnimState = Anim::Swing;
        }

        bool p2CanHit = (p2.swinging || p2.dashing) &&
                         p2.hitCooldown <= 0.f &&
                         ball.getPosition().y < netY - 25.f;
        if (p2CanHit && ballB.findIntersection(p2.getHitbox())) {
            float dirX = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dirX = -1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dirX =  1.f;
            hitToP1Field(dirX, p2.dashing);
            p2.swinging    = false;
            p2.hitCooldown = p2.hitCooldownMax;
            p2SwingAnimFrame = 0; p2SwingAnimTimer = 0.f;
            p2SwingAnimDone  = false; p2AnimState = Anim::Swing;
        }

        ball.updateSprite(dt);

        // ── Score animation timers ────────────────────────────────────────────
        if (scoreState != ScoreState::None) {
            scoreStateTimer -= dt;
            if (scoreState == ScoreState::P1Scored) {
                p1ScoreFrameTimer += dt;
                if (p1ScoreFrameTimer >= p1ScoreFrameDur) {
                    p1ScoreFrameTimer -= p1ScoreFrameDur;
                    p1ScoreFrame = 1 - p1ScoreFrame;
                }
            }
            if (scoreState == ScoreState::P2Scored) {
                p2ScoreFrameTimer += dt;
                if (p2ScoreFrameTimer >= p2ScoreFrameDur) {
                    p2ScoreFrameTimer -= p2ScoreFrameDur;
                    p2ScoreFrame = 1 - p2ScoreFrame;
                }
            }
            if (scoreStateTimer <= 0.f)
                scoreState = ScoreState::None;
        }
    }

    // ---- Win screen ----
    void drawWinScreen() {
        const float W = bgW, H = bgH;
        const float cx = W * 0.5f, cy = H * 0.5f;

        if (winnerBgSprite) {
            // Use the provided winner background image
            window.draw(*winnerBgSprite);
        } else {
            // Fallback: deep navy overlay + slow sunburst
            sf::RectangleShape overlay({W, H});
            overlay.setFillColor(sf::Color(8, 14, 42, 210));
            window.draw(overlay);

            const int   rayCount   = 12;
            const float outerR     = H * 0.72f;
            const float innerR     = H * 0.18f;
            const float rayHalfAng = 3.14159265f / rayCount;
            float angleRad = burstAngle * 3.14159265f / 180.f;
            for (int i = 0; i < rayCount; ++i) {
                float a0 = angleRad + i * (2.f * 3.14159265f / rayCount);
                float a1 = a0 + rayHalfAng * 0.55f;
                uint8_t alpha = (i % 2 == 0) ? 40 : 22;
                sf::ConvexShape ray(3);
                ray.setPoint(0, {cx, cy});
                ray.setPoint(1, {cx + cos(a0) * outerR, cy + sin(a0) * outerR});
                ray.setPoint(2, {cx + cos(a1) * outerR, cy + sin(a1) * outerR});
                ray.setFillColor(sf::Color(255, 210, 60, alpha));
                window.draw(ray);
            }
            float glowR = innerR * (0.85f + flashPhase * 0.15f);
            sf::CircleShape glow(glowR);
            glow.setOrigin({glowR, glowR});
            glow.setPosition({cx, cy});
            uint8_t glowAlpha = static_cast<uint8_t>(60 + flashPhase * 60.f);
            glow.setFillColor(sf::Color(255, 220, 80, glowAlpha));
            window.draw(glow);
        }

        // Score panel — sits in the lower third, over the court area of the image
        // boxH is generous to accommodate pixel font's taller line metrics
        const float boxW = 380.f, boxH = 110.f;
        sf::RectangleShape scoreBox({boxW, boxH});
        scoreBox.setFillColor(sf::Color(10, 10, 30, 215));
        scoreBox.setOutlineColor(sf::Color(255, 200, 50, 200));
        scoreBox.setOutlineThickness(3.f);
        scoreBox.setOrigin({boxW / 2.f, boxH / 2.f});
        scoreBox.setPosition({cx, H * 0.82f});
        window.draw(scoreBox);

        // Score panel text using pixel font — sized to match name entry screen
        if (font.getInfo().family != "") {
            auto makeScore = [&](const std::string& name, int score,
                                 sf::Color col, float yOff) {
                // Size 14 keeps pixel font readable inside the score box
                sf::Text t(font, name + ": " + to_string(score), 14);
                t.setFillColor(col);
                t.setOutlineColor(sf::Color(0, 0, 0, 180));
                t.setOutlineThickness(2.f);
                sf::FloatRect b = t.getLocalBounds();
                t.setOrigin({b.position.x + b.size.x / 2.f,
                             b.position.y + b.size.y / 2.f});
                t.setPosition({cx, H * 0.82f + yOff});
                window.draw(t);
            };
            makeScore(p1Name, score1, sf::Color(100, 220, 255), -22.f);
            makeScore(p2Name, score2, sf::Color(255, 140, 100),  22.f);
        }

        // Flashing winner name — same gold + dark-outline style as name entry title
        if (font.getInfo().family != "") {
            // Size 32: large enough to be prominent, fits pixel font without overflow
            sf::Text wt(font, winnerName + " WINS!", 32);
            uint8_t r = 255;
            uint8_t g = static_cast<uint8_t>(160 + flashPhase * 95.f);
            uint8_t b = static_cast<uint8_t>(20  + flashPhase * 30.f);
            wt.setFillColor(sf::Color(r, g, b, 255));
            wt.setOutlineColor(sf::Color(60, 30, 0, 220));
            wt.setOutlineThickness(4.f);
            sf::FloatRect wb = wt.getLocalBounds();
            wt.setOrigin({wb.position.x + wb.size.x / 2.f,
                          wb.position.y + wb.size.y / 2.f});
            float bob = sin(flashTimer * 2.8f) * 5.f;
            wt.setPosition({cx, H * 0.58f + bob});
            window.draw(wt);
        }

        // "PRESS R TO PLAY AGAIN" — same style as name entry hint text
        if (restartText) {
            sf::Text rt(*restartText);
            rt.setCharacterSize(10);        // matches hint size from name entry
            rt.setFillColor(sf::Color(180, 180, 200, 210));
            rt.setOutlineColor(sf::Color(0, 0, 0, 160));
            rt.setOutlineThickness(1.5f);
            sf::FloatRect rb = rt.getLocalBounds();
            rt.setOrigin({rb.position.x + rb.size.x / 2.f,
                          rb.position.y + rb.size.y / 2.f});
            rt.setPosition({cx, H * 0.95f});
            window.draw(rt);
        }
    }

    // ---- Draw ----
    void draw() {
        window.clear();
        if (bgCourtSprite)         window.draw(*bgCourtSprite);
        else if (backgroundSprite) window.draw(*backgroundSprite);

        // P2 swing circle
        if (p2.swinging) {
            p2SwingCircle.setPosition(p2.getPosition());
            window.draw(p2SwingCircle);
        }

        // During a score animation, replace the base sprites with
        // the appropriate score/sigh sprites so characters don't duplicate.
        if (scoreState == ScoreState::P1Scored) {
            // P1 celebrating — P2 sighing; suppress both base sprites
            if (p1ScoreSprite1 && p1ScoreSprite2 && p1SighSprite && p2SighSprite) {
                // drawn below after ball
            }
        } else if (scoreState == ScoreState::P2Scored) {
            // P2 celebrating — P1 sighing; suppress both base sprites
            if (p2ScoreSprite && p2ScoreSprite2 && p1SighSprite && p2SighSprite) {
                // drawn below after ball
            }
        } else {
            if (p1Sprite) window.draw(*p1Sprite);
            if (p2Sprite) window.draw(*p2Sprite);
        }

        ball.draw(window);

        // Draw score/sigh animation sprites on top of ball
        if (scoreState == ScoreState::P1Scored) {
            if (p1ScoreSprite1 && p1ScoreSprite2) {
                if (p1ScoreFrame == 0) window.draw(*p1ScoreSprite1);
                else                   window.draw(*p1ScoreSprite2);
            }
            if (p2SighSprite) window.draw(*p2SighSprite);
        } else if (scoreState == ScoreState::P2Scored) {
            if (p2ScoreSprite && p2ScoreSprite2) {
                if (p2ScoreFrame == 0) window.draw(*p2ScoreSprite);
                else                   window.draw(*p2ScoreSprite2);
            }
            if (p1SighSprite) window.draw(*p1SighSprite);
        }

        window.draw(p1ScoreBox);
        window.draw(p2ScoreBox);
        if (score1Text) window.draw(*score1Text);
        if (score2Text) window.draw(*score2Text);
        if (serveText)  window.draw(*serveText);

        if (p1PosText) {
            p1PosText->setString(p1Name + " (" + to_string((int)p1.getPosition().x) +
                                ", " + to_string((int)p1.getPosition().y) + ")");
            window.draw(*p1PosText);
        }
        if (p2PosText) {
            p2PosText->setString(p2Name + " (" + to_string((int)p2.getPosition().x) +
                                ", " + to_string((int)p2.getPosition().y) + ")");
            window.draw(*p2PosText);
        }

        if (gameOver) {
            drawWinScreen();
        }

        window.display();
    }

    // ---- Main loop ----
    void run() {
        sf::Clock clock;
        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            dt = min(dt, 0.033f);
            handleEvents();
            if (!gameOver) {
                updatePlayers(dt);
                updateBall(dt);
            } else {
                flashTimer += dt;
                flashPhase  = (sin(flashTimer * 3.5f) + 1.f) * 0.5f; // 0..1
                burstAngle += dt * 3.f;
                // Keep score animation ticking even on game-over screen
                if (scoreState != ScoreState::None) {
                    scoreStateTimer -= dt;
                    if (scoreState == ScoreState::P1Scored) {
                        p1ScoreFrameTimer += dt;
                        if (p1ScoreFrameTimer >= p1ScoreFrameDur) {
                            p1ScoreFrameTimer -= p1ScoreFrameDur;
                            p1ScoreFrame = 1 - p1ScoreFrame;
                        }
                    }
                    if (scoreState == ScoreState::P2Scored) {
                        p2ScoreFrameTimer += dt;
                        if (p2ScoreFrameTimer >= p2ScoreFrameDur) {
                            p2ScoreFrameTimer -= p2ScoreFrameDur;
                            p2ScoreFrame = 1 - p2ScoreFrame;
                        }
                    }
                    if (scoreStateTimer <= 0.f) scoreState = ScoreState::None;
                }
            }
            draw();
        }
    }
};

// ===================== MAIN =====================
int main() {
    Game game;
    if (!game.init()) return 1;
    game.showSplash();
    game.showNameEntry();
    game.run();
    return 0;
}