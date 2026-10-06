"""Review-only Tasks mockup; illustrative data, no firmware changes."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "preview"
S = 6
COLORS = {"Personal": (55, 237, 156), "Work": (75, 159, 255),
          "Priority": (255, 87, 103), "Project": (184, 120, 255)}
# Already ordered by deadline, including the completed task.
PAGES = [
    [("Prepare project report", "09:00", "Project"),
     ("Team meeting", "10:30", "Work"),
     ("Review PCB design", "14:00", "Priority"),
     ("Plan training session", "16:00", "Personal")],
    [("Send project update", "18:00", "Work"),
     ("Order PCB components", "07/10/2026", "Project"),
     ("Review power budget", "08/10/2026", "Priority"),
     ("Plan weekend", "10/10/2026", "Personal")],
]


def icon(dr, kind, x, y, color):
    """Draw reference-style line icons in a native 16px square."""
    def box(a, b, c, d):
        return ((x+a)*S, (y+b)*S, (x+c)*S, (y+d)*S)
    def line(points):
        dr.line([((x+a)*S, (y+b)*S) for a, b in points], fill=color,
                width=round(1.35*S), joint="curve")
    def ellipse(bounds):
        dr.ellipse(box(*bounds), outline=color, width=round(1.35*S))
    if kind == "Project":
        dr.rounded_rectangle(box(1, 5, 15, 14), radius=2*S,
                             outline=color, width=round(1.35*S))
        line([(5, 5), (5, 2), (11, 2), (11, 5)])
        line([(1, 9), (15, 9)])
        line([(8, 8), (8, 11)])
    elif kind == "Personal":
        ellipse((5, 1, 11, 7))
        dr.arc(box(2, 9, 14, 21), 180, 360, fill=color, width=round(1.35*S))
        line([(2, 15), (14, 15)])
    elif kind == "Work":
        ellipse((6, 1, 11, 6))
        ellipse((0.5, 3, 4.5, 7))
        ellipse((12, 3, 16, 7))
        dr.arc(box(4, 9, 13, 18), 180, 360, fill=color, width=round(1.35*S))
        dr.arc(box(-1, 10, 5, 18), 180, 280, fill=color, width=round(1.35*S))
        dr.arc(box(12, 10, 18, 18), 260, 360, fill=color, width=round(1.35*S))
        line([(4, 14), (13, 14)])
    elif kind == "Priority":
        ellipse((1, 1, 15, 15))
        line([(8, 4), (8, 9)])
        dr.ellipse(box(7.3, 11.5, 8.7, 12.9), fill=color)
    elif kind == "Clock":
        ellipse((1, 1, 15, 15))
        line([(8, 4), (8, 8), (11, 10)])


def badge(category):
    width, height = 101, 24
    color = COLORS[category]
    image = Image.new("RGBA", (width*S, height*S))
    mask = Image.new("L", image.size)
    md = ImageDraw.Draw(mask)
    bounds = (S, S, (width-1)*S, (height-1)*S)
    md.rounded_rectangle(bounds, radius=12*S, fill=255)
    # A saturated glass fill, brighter at the top; text and border stay sharp.
    gradient = Image.new("RGBA", image.size)
    gd = ImageDraw.Draw(gradient)
    for py in range(height*S):
        factor = 0.34 - 0.15*py/(height*S-1)
        gd.line((0, py, width*S, py), fill=tuple(round(c*factor) for c in color)+(255,))
    image.paste(gradient, (0, 0), mask)
    dr = ImageDraw.Draw(image)
    dr.rounded_rectangle(bounds, radius=12*S, outline=(*color, 255), width=S)
    ink = tuple(round(c*0.45+255*0.55) for c in color)
    icon(dr, category, 9, 4, ink)
    font = ImageFont.truetype(str(ROOT / "fonts/Poppins-Medium.ttf"), 11*S)
    dr.text((32*S, 12*S), category, font=font, fill=ink, anchor="lm")
    return image


def render(page, selected, completed=False):
    base = ROOT.parent / "TouchGFX/assets/images/aura"
    board = Image.open(base.parent / "board.png").convert("RGBA").transpose(Image.Transpose.ROTATE_270)
    board.putalpha(board.getchannel("A").point(lambda a: round(a * 0.4)))
    canvas = Image.new("RGBA", (480, 480), (0, 0, 0, 255))
    canvas.alpha_composite(board)
    canvas = canvas.resize((480*S, 480*S), Image.Resampling.LANCZOS)
    dr = ImageDraw.Draw(canvas)
    def text(x, y, value, size, color=(235, 248, 255), anchor="lt"):
        font = ImageFont.truetype(str(ROOT / "fonts/Poppins-Medium.ttf"), size*S)
        dr.text((x*S, y*S), value, font=font, fill=color, anchor=anchor)
    text(240, 60, "TASKS", 25, anchor="mt")
    text(240, 98, f"{page*4+1}-{page*4+4} OF 8  /  DEADLINE", 11,
         (148, 194, 218), "mt")
    text(89, 76, "<", 22, (85, 218, 255))
    for row, (title, deadline, category) in enumerate(PAGES[page]):
        y = 133 + row*63
        color = COLORS[category]
        dr.rounded_rectangle((65*S, y*S, 415*S, (y+57)*S), radius=10*S,
                             fill=(5, 19, 28, 215), outline=(28, 72, 96), width=S)
        if row == selected:
            glow = Image.new("RGBA", canvas.size)
            gd = ImageDraw.Draw(glow)
            gd.ellipse((76*S, (y+16)*S, 103*S, (y+43)*S), fill=(35, 221, 255, 210))
            canvas.alpha_composite(glow.filter(ImageFilter.GaussianBlur(6*S)))
            dr = ImageDraw.Draw(canvas)
            dr.ellipse((80*S, (y+20)*S, 99*S, (y+39)*S), fill=(100, 246, 255), outline=(205, 255, 255), width=S)
        else:
            dr.ellipse((81*S, (y+21)*S, 98*S, (y+38)*S), outline=(156, 210, 234), width=S)
        done = completed and row == 1 and page == 0
        title_color = (139, 175, 199) if done else (235, 248, 255)
        text(112, y+8, title, 14, title_color)
        if done:
            font = ImageFont.truetype(str(ROOT / "fonts/Poppins-Medium.ttf"), 14*S)
            # Match the title's anchor and derive the strike from actual ink
            # bounds, rather than a fixed offset that reads as an underline.
            left, top, right, bottom = dr.textbbox((112*S, (y+8)*S),
                                                  title, font=font, anchor="lt")
            strike_y = (top + bottom) // 2
            dr.line((left, strike_y, right, strike_y), fill=title_color, width=S)
            dr.line((84*S, (y+29)*S, 88*S, (y+33)*S, 95*S, (y+25)*S), fill=(0, 60, 83), width=2*S)
        icon(dr, "Clock", 112, y+33, (160, 205, 238))
        text(135, y+34, deadline, 11, (160, 205, 238))
        canvas.alpha_composite(badge(category), (303*S, (y+29)*S))
        dr = ImageDraw.Draw(canvas)
    text(114, 413, "<", 23, (110, 231, 255))
    text(366, 413, ">", 23, (110, 231, 255))
    text(240, 423, f"{page+1} / 2", 12, (153, 200, 225), "mt")
    text(240, 450, "CLICK TO COMPLETE", 10, (130, 180, 203), "mt")
    canvas = canvas.resize((480, 480), Image.Resampling.LANCZOS)
    rim = Image.open(base / "rim/rim.png").convert("RGBA").transpose(Image.Transpose.ROTATE_270)
    canvas.alpha_composite(rim)
    # Reuse the shared status header assets and framebuffer positions.
    # These levels are illustrative; firmware reads live board status.
    header = Image.new("RGBA", (480, 480))
    header.alpha_composite(Image.open(base / "status/wifi_3.png").convert("RGBA"), (31, 283))
    hd = ImageDraw.Draw(header)
    hd.rectangle((38, 187, 46, 203), fill=(86, 210, 255))
    header.alpha_composite(Image.open(base / "status/batt_frame.png").convert("RGBA"), (35, 176))
    logo = next(base.rglob("logo_00.png"))
    header.alpha_composite(Image.open(logo).convert("RGBA"), (20, 217))
    canvas.alpha_composite(header.transpose(Image.Transpose.ROTATE_270))
    return canvas.convert("RGB")


def main():
    OUT.mkdir(exist_ok=True)
    frames = [render(0, 0), render(0, 1), render(0, 1, True), render(0, 2, True),
              render(0, 3, True), render(1, 0)]
    frames[2].save(OUT / "tasks_menu.png")
    details = Image.new("RGBA", (145*S, 170*S), (5, 18, 28, 255))
    for row, category in enumerate(("Project", "Work", "Priority", "Personal")):
        details.alpha_composite(badge(category), (22*S, (8+row*32)*S))
    dd = ImageDraw.Draw(details)
    icon(dd, "Clock", 24, 145, (160, 205, 238))
    dd.text((48*S, 153*S), "16:00", font=ImageFont.truetype(str(ROOT / "fonts/Poppins-Medium.ttf"), 13*S),
            fill=(160, 205, 238), anchor="lm")
    details.convert("RGB").resize((290, 340), Image.Resampling.LANCZOS).save(OUT / "tasks_tags_detail.png")
    frames[0].save(OUT / "tasks_menu.gif", save_all=True, append_images=frames[1:],
                   duration=1100, loop=0)
    print(OUT / "tasks_menu.png")


if __name__ == "__main__":
    main()
