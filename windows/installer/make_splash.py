"""Generates installer/splash.bmp: a terminal-window mockup used as the
installer splash screen (shown via the Splash:: NSIS plugin)."""

from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 450, 280

BG = (10, 10, 12)
WINDOW_BG = (24, 24, 26)
WINDOW_BORDER = (60, 60, 64)
TITLEBAR_BG = (40, 40, 44)
DOT_RED = (255, 95, 86)
DOT_YELLOW = (255, 189, 46)
DOT_GREEN = (39, 201, 63)
PROMPT_GRAY = (130, 130, 130)
TEXT_WHITE = (235, 235, 235)
TEXT_GREEN = (80, 220, 100)
TITLEBAR_TEXT = (180, 180, 180)

FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONT_MONO_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"


def main():
    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(img)

    # Terminal window
    win_box = (16, 16, WIDTH - 16, HEIGHT - 16)
    draw.rounded_rectangle(win_box, radius=10, fill=WINDOW_BG, outline=WINDOW_BORDER, width=2)

    # Title bar
    titlebar_box = (16, 16, WIDTH - 16, 46)
    draw.rounded_rectangle(titlebar_box, radius=10, fill=TITLEBAR_BG)
    draw.rectangle((16, 36, WIDTH - 16, 46), fill=TITLEBAR_BG)  # square off bottom corners

    # Traffic-light dots
    for i, color in enumerate((DOT_RED, DOT_YELLOW, DOT_GREEN)):
        cx = 38 + i * 20
        cy = 31
        r = 6
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)

    title_font = ImageFont.truetype(FONT_MONO, 13)
    title_text = "star_term — ssh"
    tw = draw.textlength(title_text, font=title_font)
    draw.text(((WIDTH - tw) / 2, 22), title_text, font=title_font, fill=TITLEBAR_TEXT)

    # Terminal body content
    mono = ImageFont.truetype(FONT_MONO, 15)
    mono_bold = ImageFont.truetype(FONT_MONO_BOLD, 15)
    big_bold = ImageFont.truetype(FONT_MONO_BOLD, 34)
    small = ImageFont.truetype(FONT_MONO, 14)

    x = 36
    y = 64

    draw.text((x, y), "$ ssh setup@star_term", font=mono, fill=PROMPT_GRAY)
    y += 32

    # Big app name with a green ">_" prompt accent
    prompt = ">_ "
    pw = draw.textlength(prompt, font=big_bold)
    draw.text((x, y), prompt, font=big_bold, fill=TEXT_GREEN)
    draw.text((x + pw, y), "star_term", font=big_bold, fill=TEXT_WHITE)
    y += 54

    draw.text((x, y), "SSH Terminal - Setup", font=mono, fill=PROMPT_GRAY)
    y += 36

    # Blinking-cursor style status line
    draw.text((x, y), "Preparing installation", font=small, fill=PROMPT_GRAY)
    status_w = draw.textlength("Preparing installation ", font=small)
    cursor_h = 16
    draw.rectangle(
        (x + status_w, y + 1, x + status_w + 9, y + 1 + cursor_h),
        fill=TEXT_GREEN,
    )

    img.save("splash.bmp")
    print(f"wrote splash.bmp ({WIDTH}x{HEIGHT})")


if __name__ == "__main__":
    main()
