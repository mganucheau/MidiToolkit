#include "UiAtoms.h"

namespace pflow {

// ── Icons ────────────────────────────────────────────────────────────────────

void drawIcon(juce::Graphics& g, const juce::String& name,
              juce::Rectangle<float> b, juce::Colour colour, float px)
{
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float s = juce::jmin(b.getWidth(), b.getHeight()) * 0.36f;
    g.setColour(colour);
    juce::PathStrokeType st(px, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;

    if (name == icons::play)
    {
        p.addTriangle(cx - s * 0.62f, cy - s, cx - s * 0.62f, cy + s, cx + s, cy);
        g.fillPath(p);
    }
    else if (name == icons::pause)
    {
        const float w = s * 0.55f;
        g.fillRoundedRectangle(cx - s * 0.8f, cy - s, w, s * 2.0f, px * 0.8f);
        g.fillRoundedRectangle(cx + s * 0.25f, cy - s, w, s * 2.0f, px * 0.8f);
    }
    else if (name == icons::stop)
    {
        g.fillRoundedRectangle(cx - s * 0.85f, cy - s * 0.85f, s * 1.7f, s * 1.7f, px);
    }
    else if (name == icons::record)
    {
        g.fillEllipse(cx - s * 0.72f, cy - s * 0.72f, s * 1.44f, s * 1.44f);
    }
    else if (name == icons::caretUp || name == icons::caretDown)
    {
        const float dir = (name == icons::caretUp) ? -1.0f : 1.0f;
        p.startNewSubPath(cx - s * 0.8f, cy - dir * s * 0.4f);
        p.lineTo(cx, cy + dir * s * 0.4f);
        p.lineTo(cx + s * 0.8f, cy - dir * s * 0.4f);
        g.strokePath(p, st);
    }
    else if (name == icons::caretLeft || name == icons::caretRight)
    {
        const float dir = (name == icons::caretLeft) ? -1.0f : 1.0f;
        p.startNewSubPath(cx - dir * s * 0.4f, cy - s * 0.8f);
        p.lineTo(cx + dir * s * 0.4f, cy);
        p.lineTo(cx - dir * s * 0.4f, cy + s * 0.8f);
        g.strokePath(p, st);
    }
    else if (name == icons::arrowsIn || name == icons::arrowsOut)
    {
        // Center line + two horizontal arrows pointing in or out.
        g.drawLine(cx, cy - s, cx, cy + s, px);
        const bool in = (name == icons::arrowsIn);
        auto arrow = [&](float side)   // side -1 = left, +1 = right
        {
            const float x0 = cx + side * s * 1.35f;
            const float x1 = cx + side * s * 0.35f;
            g.drawLine(x0, cy, x1, cy, px);
            const float hx = in ? x1 : x0;
            const float hd = in ? side : -side;
            juce::Path h;
            h.startNewSubPath(hx + hd * s * 0.45f, cy - s * 0.45f);
            h.lineTo(hx, cy);
            h.lineTo(hx + hd * s * 0.45f, cy + s * 0.45f);
            g.strokePath(h, st);
        };
        arrow(-1.0f);
        arrow(1.0f);
    }
    else if (name == icons::dragOut)
    {
        // Arrow entering a tray: the clip leaving the browser for a DAW track.
        g.drawRoundedRectangle(cx - s * 0.05f, cy - s * 0.9f, s * 1.15f, s * 1.8f, s * 0.2f, px);
        g.drawLine(cx - s * 1.35f, cy, cx + s * 0.15f, cy, px);
        juce::Path head;
        head.startNewSubPath(cx - s * 0.25f, cy - s * 0.42f);
        head.lineTo(cx + s * 0.28f, cy);
        head.lineTo(cx - s * 0.25f, cy + s * 0.42f);
        g.strokePath(head, st);
    }
    else if (name == icons::scissors)
    {
        g.drawLine(cx - s * 0.15f, cy - s * 0.1f, cx + s, cy - s * 0.9f, px);
        g.drawLine(cx - s * 0.15f, cy + s * 0.1f, cx + s, cy + s * 0.9f, px);
        g.drawEllipse(cx - s * 1.05f, cy - s * 0.85f, s * 0.75f, s * 0.75f, px);
        g.drawEllipse(cx - s * 1.05f, cy + s * 0.1f, s * 0.75f, s * 0.75f, px);
    }
    else if (name == icons::sliders)
    {
        // Horizontal adjustment tracks (Filters).
        for (int i = 0; i < 3; ++i)
        {
            const float y = cy - s + (float) i * s;
            g.drawLine(cx - s, y, cx + s, y, px);
            const float kx = cx + (i == 0 ? s * 0.4f : i == 1 ? -s * 0.35f : s * 0.05f);
            g.fillEllipse(kx - px, y - px, px * 2.0f, px * 2.0f);
        }
    }
    else if (name == icons::eq)
    {
        // Vertical EQ / mixer faders — the usual Effects glyph.
        const float barW = juce::jmax(px * 1.1f, s * 0.28f);
        const float gap = s * 0.55f;
        const float heights[3] = { s * 0.55f, s * 1.15f, s * 0.8f };
        for (int i = 0; i < 3; ++i)
        {
            const float x = cx + ((float) i - 1.0f) * gap - barW * 0.5f;
            const float h = heights[i];
            const float y = cy + s - h;
            g.fillRoundedRectangle(x, y, barW, h, barW * 0.35f);
        }
    }
    else if (name == icons::folder || name == icons::folderOpen)
    {
        p.startNewSubPath(cx - s, cy + s * 0.7f);
        p.lineTo(cx - s, cy - s * 0.55f);
        p.lineTo(cx - s * 0.25f, cy - s * 0.55f);
        p.lineTo(cx - s * 0.02f, cy - s * 0.25f);
        p.lineTo(cx + s, cy - s * 0.25f);
        p.lineTo(cx + s, cy + s * 0.7f);
        p.closeSubPath();
        g.strokePath(p, st);
    }
    else if (name == icons::star || name == icons::starOutline)
    {
        for (int i = 0; i < 10; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 10.0f
                            - juce::MathConstants<float>::halfPi;
            const float r = (i % 2 == 0) ? s : s * 0.45f;
            const float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        p.closeSubPath();
        if (name == icons::starOutline)
            g.strokePath(p, st);
        else
            g.fillPath(p);
    }
    else if (name == icons::heart || name == icons::heartOutline)
    {
        // Classic heart from two lobes + pointed tip.
        p.startNewSubPath(cx, cy + s * 0.85f);
        p.cubicTo(cx + s * 1.15f, cy + s * 0.1f,
                  cx + s * 1.05f, cy - s * 0.75f,
                  cx, cy - s * 0.25f);
        p.cubicTo(cx - s * 1.05f, cy - s * 0.75f,
                  cx - s * 1.15f, cy + s * 0.1f,
                  cx, cy + s * 0.85f);
        p.closeSubPath();
        if (name == icons::heartOutline)
            g.strokePath(p, st);
        else
            g.fillPath(p);
    }
    else if (name == icons::bookmark || name == icons::bookmarkFill)
    {
        // Vertical bookmark ribbon with a V notch at the bottom.
        p.startNewSubPath(cx - s * 0.55f, cy - s * 0.95f);
        p.lineTo(cx + s * 0.55f, cy - s * 0.95f);
        p.lineTo(cx + s * 0.55f, cy + s * 0.95f);
        p.lineTo(cx, cy + s * 0.45f);
        p.lineTo(cx - s * 0.55f, cy + s * 0.95f);
        p.closeSubPath();
        if (name == icons::bookmarkFill)
            g.fillPath(p);
        else
            g.strokePath(p, st);
    }
    else if (name == icons::plus)
    {
        g.drawLine(cx - s * 0.85f, cy, cx + s * 0.85f, cy, px);
        g.drawLine(cx, cy - s * 0.85f, cx, cy + s * 0.85f, px);
    }
    else if (name == icons::minus)
    {
        g.drawLine(cx - s * 0.85f, cy, cx + s * 0.85f, cy, px);
    }
    else if (name == icons::x)
    {
        g.drawLine(cx - s * 0.7f, cy - s * 0.7f, cx + s * 0.7f, cy + s * 0.7f, px);
        g.drawLine(cx - s * 0.7f, cy + s * 0.7f, cx + s * 0.7f, cy - s * 0.7f, px);
    }
    else if (name == icons::undo || name == icons::refresh)
    {
        p.addCentredArc(cx, cy, s * 0.85f, s * 0.85f, 0.0f, 0.6f,
                        juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath(p, st);
        juce::Path h;
        const float ax = cx + s * 0.85f * std::sin(0.6f);
        const float ay = cy - s * 0.85f * std::cos(0.6f);
        h.startNewSubPath(ax - s * 0.5f, ay - s * 0.05f);
        h.lineTo(ax, ay);
        h.lineTo(ax + s * 0.15f, ay - s * 0.55f);
        g.strokePath(h, st);
    }
    else if (name == icons::zoomIn || name == icons::zoomOut)
    {
        g.drawEllipse(cx - s, cy - s, s * 1.5f, s * 1.5f, px);
        g.drawLine(cx + s * 0.15f, cy + s * 0.15f, cx + s * 0.95f, cy + s * 0.95f, px);
        const float mx = cx - s * 0.25f, my = cy - s * 0.25f;
        g.drawLine(mx - s * 0.4f, my, mx + s * 0.4f, my, px);
        if (name == icons::zoomIn)
            g.drawLine(mx, my - s * 0.4f, mx, my + s * 0.4f, px);
    }
    else if (name == icons::noteBass)
    {
        p.startNewSubPath(cx - s, cy);
        p.cubicTo(cx - s * 0.55f, cy - s * 1.4f, cx - s * 0.1f, cy + s * 1.4f, cx + s * 0.35f, cy);
        p.cubicTo(cx + s * 0.6f, cy - s * 0.8f, cx + s * 0.85f, cy + s * 0.8f, cx + s, cy);
        g.strokePath(p, st);
    }
    else if (name == icons::noteKeys)
    {
        // Mini piano: white key frame + two black keys.
        auto r = juce::Rectangle<float>(cx - s * 1.05f, cy - s * 0.75f, s * 2.1f, s * 1.5f);
        g.drawRoundedRectangle(r, px * 0.9f, px);
        const float keyW = r.getWidth() / 3.0f;
        g.drawLine(r.getX() + keyW, r.getY(), r.getX() + keyW, r.getBottom(), px * 0.85f);
        g.drawLine(r.getX() + keyW * 2.0f, r.getY(), r.getX() + keyW * 2.0f, r.getBottom(), px * 0.85f);
        const float bkW = keyW * 0.55f, bkH = r.getHeight() * 0.58f;
        g.fillRoundedRectangle(r.getX() + keyW - bkW * 0.5f, r.getY(), bkW, bkH, px * 0.4f);
        g.fillRoundedRectangle(r.getX() + keyW * 2.0f - bkW * 0.5f, r.getY(), bkW, bkH, px * 0.4f);
    }
    else if (name == icons::noteDrums)
    {
        g.drawEllipse(cx - s, cy - s * 0.55f, s * 2.0f, s * 0.75f, px);
        g.drawLine(cx - s, cy - s * 0.18f, cx - s, cy + s * 0.6f, px);
        g.drawLine(cx + s, cy - s * 0.18f, cx + s, cy + s * 0.6f, px);
        juce::Path arc;
        arc.addCentredArc(cx, cy + s * 0.28f, s, s * 0.4f, 0.0f,
                          juce::MathConstants<float>::halfPi,
                          juce::MathConstants<float>::pi * 1.5f, true);
        g.strokePath(arc, st);
    }
    else if (name == icons::sidebar || name == icons::sidebarRight)
    {
        auto r = juce::Rectangle<float>(cx - s, cy - s * 0.8f, s * 2.0f, s * 1.6f);
        g.drawRoundedRectangle(r, px, px);
        const float x = (name == icons::sidebarRight) ? (cx + s * 0.3f) : (cx - s * 0.3f);
        g.drawLine(x, cy - s * 0.8f, x, cy + s * 0.8f, px);
    }
    else if (name == icons::sync)
    {
        // Two linked loops (host ↔ plugin) — reads as “sync”, not refresh.
        const float rr = s * 0.55f;
        g.drawEllipse(cx - s * 0.95f, cy - rr, rr * 2.0f, rr * 2.0f, px);
        g.drawEllipse(cx - s * 0.05f, cy - rr, rr * 2.0f, rr * 2.0f, px);
    }
    else if (name == icons::lockOpen || name == icons::lockClosed)
    {
        const bool closed = (name == icons::lockClosed);
        auto body = juce::Rectangle<float>(cx - s * 0.75f, cy - s * 0.15f, s * 1.5f, s * 1.05f);
        g.drawRoundedRectangle(body, px, px);
        juce::Path shackle;
        const float top = closed ? cy - s * 0.95f : cy - s * 1.15f;
        shackle.startNewSubPath(cx - s * 0.45f, body.getY());
        shackle.lineTo(cx - s * 0.45f, top + s * 0.35f);
        shackle.addCentredArc(cx, top + s * 0.35f, s * 0.45f, s * 0.35f, 0.0f,
                              -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, false);
        if (closed)
            shackle.lineTo(cx + s * 0.45f, body.getY());
        else
            shackle.lineTo(cx + s * 0.45f, body.getY() - s * 0.35f);
        g.strokePath(shackle, st);
        g.fillEllipse(cx - px, cy + s * 0.3f, px * 2.0f, px * 2.0f);
    }
    else if (name == icons::foldRows)
    {
        // Rows collapsing together: three lines with arrows pointing inward.
        g.drawLine(cx - s, cy - s * 0.85f, cx + s, cy - s * 0.85f, px);
        g.drawLine(cx - s, cy + s * 0.85f, cx + s, cy + s * 0.85f, px);
        g.drawLine(cx - s, cy, cx + s, cy, px);
        juce::Path a1, a2;
        a1.startNewSubPath(cx - s * 0.3f, cy - s * 0.55f);
        a1.lineTo(cx, cy - s * 0.28f);
        a1.lineTo(cx + s * 0.3f, cy - s * 0.55f);
        a2.startNewSubPath(cx - s * 0.3f, cy + s * 0.55f);
        a2.lineTo(cx, cy + s * 0.28f);
        a2.lineTo(cx + s * 0.3f, cy + s * 0.55f);
        g.strokePath(a1, st);
        g.strokePath(a2, st);
    }
    else if (name == icons::save || name == icons::saveFill)
    {
        auto disk = juce::Rectangle<float>(cx - s * 0.85f, cy - s * 0.95f, s * 1.7f, s * 1.9f);
        auto shutter = juce::Rectangle<float>(disk.getX() + disk.getWidth() * 0.28f,
                                              disk.getY() + px,
                                              disk.getWidth() * 0.44f,
                                              disk.getHeight() * 0.28f);
        auto label = juce::Rectangle<float>(disk.getX() + disk.getWidth() * 0.22f,
                                            disk.getBottom() - disk.getHeight() * 0.38f,
                                            disk.getWidth() * 0.56f, disk.getHeight() * 0.20f);
        if (name == icons::saveFill)
        {
            juce::Path body;
            body.addRoundedRectangle(disk, px * 0.8f);
            body.setUsingNonZeroWinding(false);
            body.addRectangle(shutter);
            g.fillPath(body);
        }
        else
        {
            g.drawRoundedRectangle(disk, px * 0.8f, px);
            g.drawRect(shutter, px);
        }
        g.fillRoundedRectangle(label, px * 0.4f);
    }
    else if (name == icons::midi)
    {
        g.drawEllipse(cx - s, cy - s, s * 2.0f, s * 2.0f, px);
        const float pr = juce::jmax(1.1f, px * 0.85f);
        const float pins[5][2] = {
            { -0.42f, -0.32f }, { 0.0f, -0.52f }, { 0.42f, -0.32f },
            { -0.26f,  0.38f }, { 0.26f,  0.38f }
        };
        for (const auto& pin : pins)
            g.fillEllipse(cx + s * pin[0] - pr, cy + s * pin[1] - pr, pr * 2.0f, pr * 2.0f);
    }
    else if (name == icons::infinity)
    {
        // Lemniscate (∞) for DAW sync.
        juce::Path lemni;
        lemni.startNewSubPath(cx - s, cy);
        lemni.cubicTo(cx - s, cy - s * 0.95f, cx - s * 0.15f, cy - s * 0.95f, cx, cy);
        lemni.cubicTo(cx + s * 0.15f, cy + s * 0.95f, cx + s, cy + s * 0.95f, cx + s, cy);
        lemni.cubicTo(cx + s, cy - s * 0.95f, cx + s * 0.15f, cy - s * 0.95f, cx, cy);
        lemni.cubicTo(cx - s * 0.15f, cy + s * 0.95f, cx - s, cy + s * 0.95f, cx - s, cy);
        g.strokePath(lemni, st);
    }
    else if (name == icons::search)
    {
        g.drawEllipse(cx - s * 0.55f, cy - s * 0.65f, s * 1.05f, s * 1.05f, px);
        g.drawLine(cx + s * 0.35f, cy + s * 0.45f, cx + s * 0.95f, cy + s * 1.05f, px * 1.2f);
    }
    else if (name == icons::moon)
    {
        // Typical site moon: solid crescent.
        juce::Path disc, cut;
        disc.addEllipse(cx - s * 0.78f, cy - s * 0.78f, s * 1.56f, s * 1.56f);
        cut.addEllipse(cx - s * 0.05f, cy - s * 0.95f, s * 1.45f, s * 1.45f);
        disc.setUsingNonZeroWinding(false);
        disc.addPath(cut);
        g.fillPath(disc);
    }
    else if (name == icons::sun)
    {
        // Typical site sun: filled disc + rays.
        g.fillEllipse(cx - s * 0.38f, cy - s * 0.38f, s * 0.76f, s * 0.76f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
            g.drawLine(cx + s * 0.58f * std::cos(a), cy + s * 0.58f * std::sin(a),
                       cx + s * 0.95f * std::cos(a), cy + s * 0.95f * std::sin(a), px * 1.15f);
        }
    }
    else if (name == icons::gear)
    {
        // Standard 6-tooth settings cog: outer teeth + rim + hub.
        juce::Path cog;
        constexpr int kTeeth = 6;
        constexpr int kSteps = kTeeth * 2;
        for (int i = 0; i < kSteps; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * ((float) i) / (float) kSteps
                             - juce::MathConstants<float>::halfPi;
            const float a1 = juce::MathConstants<float>::twoPi * ((float) i + 0.55f) / (float) kSteps
                             - juce::MathConstants<float>::halfPi;
            const float r = (i % 2 == 0) ? s * 0.98f : s * 0.68f;
            const float x0 = cx + r * std::cos(a0), y0 = cy + r * std::sin(a0);
            const float x1 = cx + r * std::cos(a1), y1 = cy + r * std::sin(a1);
            if (i == 0) cog.startNewSubPath(x0, y0);
            else        cog.lineTo(x0, y0);
            cog.lineTo(x1, y1);
        }
        cog.closeSubPath();
        g.strokePath(cog, st);
        g.drawEllipse(cx - s * 0.42f, cy - s * 0.42f, s * 0.84f, s * 0.84f, px);
        g.drawEllipse(cx - s * 0.18f, cy - s * 0.18f, s * 0.36f, s * 0.36f, px);
    }
    else if (name == icons::grid)
    {
        const float cell = s * 0.72f;
        const float gap = s * 0.28f;
        const float x0 = cx - cell - gap * 0.5f;
        const float y0 = cy - cell - gap * 0.5f;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
                g.drawRoundedRectangle(x0 + (float) col * (cell + gap),
                                       y0 + (float) row * (cell + gap),
                                       cell, cell, px * 0.6f, px);
    }
    else if (name == icons::subfolders)
    {
        // Back folder + front folder, reads as “include nested folders”.
        auto back = juce::Rectangle<float>(cx - s * 1.05f, cy - s * 0.55f, s * 1.55f, s * 1.15f);
        g.drawRoundedRectangle(back, px * 0.7f, px);
        g.drawLine(back.getX(), back.getY() + s * 0.28f, back.getRight(), back.getY() + s * 0.28f, px);
        auto front = juce::Rectangle<float>(cx - s * 0.35f, cy - s * 0.15f, s * 1.4f, s * 1.15f);
        g.drawRoundedRectangle(front, px * 0.7f, px);
        g.drawLine(front.getX(), front.getY() + s * 0.28f, front.getRight(), front.getY() + s * 0.28f, px);
    }
    else if (name == icons::more)
    {
        // Horizontal ellipsis — three equal dots.
        const float d = s * 0.28f;
        const float gap = s * 0.55f;
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse(cx + (float) i * gap - d * 0.5f, cy - d * 0.5f, d, d);
    }
}

// ── IconBtn ──────────────────────────────────────────────────────────────────

IconBtn::IconBtn(const juce::String& iconName, const juce::String& tip)
    : juce::Button(iconName), icon(iconName)
{
    if (tip.isNotEmpty())
        setTooltip(tip);
    setWantsKeyboardFocus(true);
}

int IconBtn::captionWidth() const
{
    // Groove toggle: control-lg square, or 8 px padding + 10 px glyph + 4 px gap + caption.
    const float sc = juce::jmax(0.5f, contentScale());
    if (caption.isEmpty())
        return juce::roundToInt(24.0f * sc);
    const int textW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(uiFont(11.0f, true), caption));
    const int padH = juce::roundToInt(8.0f * sc);
    const int iconW = juce::roundToInt(10.0f * sc);
    const int gap = juce::roundToInt(4.0f * sc);
    return padH + iconW + gap + textW + padH;
}

void IconBtn::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    // No square chrome — toggles read through glyph colour only.
    juce::ignoreUnused(ghost, headerChrome);

    const juce::Colour litCol = activeTint.isTransparent() ? ds::acc() : activeTint;
    juce::Colour col;
    if (!isEnabled())
        col = ds::inkFaint();
    else if (active)
        col = down ? litCol.darker(0.12f) : litCol;
    else if (over || down)
        col = ds::ink();
    else
        col = headerChrome ? ds::inkMuted() : ds::ink();

    const float stroke = juce::jlimit(1.2f, 1.55f, 1.4f * contentScale());
    const float glyphPx = juce::jmin(b.getHeight() - 4.0f,
                                     (float) metrics::chromeIconGlyphSize() * iconScale);

    if (caption.isNotEmpty())
    {
        const float sc = juce::jmax(0.5f, contentScale());
        const auto font = uiFont(11.0f, true);
        const float padL = 8.0f * sc, iconW = glyphPx, gap = 4.0f * sc;
        auto iconR = juce::Rectangle<float>(b.getX() + padL, b.getCentreY() - iconW * 0.5f, iconW, iconW);
        drawIcon(g, icon, iconR, col, stroke);
        g.setFont(font);
        g.setColour(col);
        g.drawText(caption, juce::Rectangle<float>(iconR.getRight() + gap, b.getY(),
                                                   b.getRight() - iconR.getRight() - gap, b.getHeight()),
                   juce::Justification::centredLeft, false);
    }
    else
    {
        drawIcon(g, icon, b.withSizeKeepingCentre(glyphPx, glyphPx), col, stroke);
    }

    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, ds::radius::sm);
}

// ── ChipBtn ──────────────────────────────────────────────────────────────────

ChipBtn::ChipBtn(const juce::String& text, const juce::String& iconName)
    : juce::Button(text), label(text), icon(iconName)
{
    setWantsKeyboardFocus(true);
}

int ChipBtn::idealWidth() const
{
    // Matches paintButton: 6 px side padding, 10 px glyphs with 4 px gaps, 11 px label.
    const auto f = mono ? monoFont(11.0f) : uiFont(11.0f, true);
    int w = (int) std::ceil(juce::GlyphArrangement::getStringWidth(f, label)) + 16;
    if (icon.isNotEmpty()) w += 14;
    if (trailingIcon.isNotEmpty()) w += 14;
    return w;
}

void ChipBtn::paintButton(juce::Graphics& g, bool over, bool down)
{
    // Groove toggle / tag: square-ish (radius-sm), edge border when unlit, accent fill when lit.
    auto b = getLocalBounds().toFloat();
    const float r = ds::radius::sm;

    if (active)
    {
        g.setColour(down ? ds::acc().darker(0.12f) : ds::acc());
        g.fillRoundedRectangle(b, r);
    }
    else
    {
        g.setColour((over || down) ? ds::surface300() : ds::surface200());
        g.fillRoundedRectangle(b, r);
        g.setColour(accentText ? ds::acc() : ds::edge());
        g.drawRoundedRectangle(b.reduced(0.5f), r, 1.0f);
    }

    juce::Colour col = active ? ds::onAccent()
                     : accentText ? ds::acc()
                                  : ds::ink();
    if (!isEnabled())
        col = ds::inkFaint();

    auto area = getLocalBounds().reduced(6, 0);
    if (icon.isNotEmpty())
    {
        auto ia = area.removeFromLeft(10).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawIcon(g, icon, ia, col, 1.3f);
        area.removeFromLeft(4);
    }
    if (trailingIcon.isNotEmpty())
    {
        auto ta = area.removeFromRight(10).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
        drawIcon(g, trailingIcon, ta, col.withAlpha(over ? 1.0f : 0.85f), 1.3f);
    }
    g.setColour(col);
    g.setFont(mono ? monoFont(11.0f) : uiFont(11.0f, true));
    g.drawFittedText(label, area, juce::Justification::centred, 1, 1.0f);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, r);
}

// ── PillToggle ───────────────────────────────────────────────────────────────

PillToggle::PillToggle(const juce::String& onText, const juce::String& offText)
    : juce::Button("sync"), onLabel(onText), offLabel(offText)
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

int PillToggle::idealWidth() const
{
    const auto f = uiFont(13.0f, true);
    const float w = juce::jmax(juce::GlyphArrangement::getStringWidth(f, onLabel),
                               juce::GlyphArrangement::getStringWidth(f, offLabel));
    return (int) std::ceil(w) + 34;
}

void PillToggle::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float r = b.getHeight() * 0.5f;
    const bool on = getToggleState();

    juce::Colour fill = on ? colours::accentSoft() : colours::elev();
    if (down) fill = fill.darker(0.1f);
    else if (over) fill = fill.brighter(0.05f);
    g.setColour(fill);
    g.fillRoundedRectangle(b, r);
    g.setColour(on ? colours::accentLine() : colours::line());
    g.drawRoundedRectangle(b, r, 1.0f);

    auto area = getLocalBounds().reduced(10, 0);
    auto dot = area.removeFromLeft(8).toFloat().withSizeKeepingCentre(6.0f, 6.0f);
    g.setColour(on ? colours::accent() : colours::text3());
    g.fillEllipse(dot);
    area.removeFromLeft(6);

    g.setColour(on ? colours::accentBright() : colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawFittedText(on ? onLabel : offLabel, area, juce::Justification::centredLeft, 1, 1.0f);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, b, r);
}

// ── MiniSwitch ───────────────────────────────────────────────────────────────

MiniSwitch::MiniSwitch(const juce::String& c) : juce::Button(c), caption(c)
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

int MiniSwitch::idealWidth() const
{
    const float capW = juce::GlyphArrangement::getStringWidth(uiFont(14.0f, true), caption);
    const float valW = juce::jmax(
        juce::GlyphArrangement::getStringWidth(monoFont(13.0f, false), onText),
        juce::GlyphArrangement::getStringWidth(monoFont(13.0f, false), offText));
    return (int) std::ceil(capW + valW) + 26 + 18;   // track + gaps
}

void MiniSwitch::paintButton(juce::Graphics& g, bool over, bool down)
{
    juce::ignoreUnused(down);
    const bool on = getToggleState();
    auto row = getLocalBounds();
    auto bounds = getLocalBounds().toFloat();

    // Caption · track · value, all on one row.
    g.setColour(on ? colours::text() : colours::text2());
    g.setFont(uiFont(14.0f, true));
    const int capW = (int) std::ceil(
        juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), caption));
    g.drawFittedText(caption, row.removeFromLeft(capW), juce::Justification::centredLeft, 1, 1.0f);
    row.removeFromLeft(7);

    auto track = row.removeFromLeft(26).toFloat().withSizeKeepingCentre(24.0f, 14.0f);
    g.setColour(on ? colours::accent() : (over ? colours::elev().brighter(0.1f) : colours::elev()));
    g.fillRoundedRectangle(track, 7.0f);
    g.setColour(on ? colours::accentInk() : colours::text3());
    const float kx = on ? track.getRight() - 12.0f : track.getX() + 2.0f;
    g.fillEllipse(kx, track.getCentreY() - 5.0f, 10.0f, 10.0f);

    row.removeFromLeft(6);
    g.setColour(on ? colours::accentBright() : colours::text3());
    g.setFont(monoFont(13.0f, true));
    g.drawFittedText(on ? onText : offText, row, juce::Justification::centredLeft, 1, 1.0f);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, bounds, 6.0f);
}

// ── Stepper ──────────────────────────────────────────────────────────────────

Stepper::Stepper()
{
    btnDown.ghost = false;
    btnUp.ghost = false;
    btnDown.onClick = [this] { setValue(value - 1); };
    btnUp.onClick = [this] { setValue(value + 1); };
    addAndMakeVisible(btnDown);
    addAndMakeVisible(btnUp);
}

void Stepper::setValue(int v, juce::NotificationType notify)
{
    v = juce::jlimit(minValue, maxValue, v);
    if (v == value) return;
    value = v;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(value);
}

void Stepper::resized()
{
    auto b = getLocalBounds();
    btnDown.setBounds(b.removeFromLeft(b.getHeight()));
    btnUp.setBounds(b.removeFromRight(b.getHeight()));
}

void Stepper::paint(juce::Graphics& g)
{
    auto mid = getLocalBounds().withTrimmedLeft(getHeight()).withTrimmedRight(getHeight());
    g.setColour(colours::elev());
    g.fillRoundedRectangle(mid.toFloat().reduced(1.0f, 2.0f), 4.0f);
    g.setColour(value != 0 ? colours::accent() : colours::text2());
    g.setFont(monoFont(14.0f, true));
    const juce::String text = format ? format(value)
                                     : (value > 0 ? "+" + juce::String(value) : juce::String(value));
    g.drawFittedText(text, mid, juce::Justification::centred, 1, 1.0f);
}

} // namespace pflow
