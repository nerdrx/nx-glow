"""Check real compositor screenshots, not a shader implementation copy."""
from pathlib import Path
from PIL import Image

out = Path(__file__).resolve().parent.parent / "test-output"
images = {name: Image.open(out / f"{name}.png").convert("RGB")
          for name in ("before", "after", "changed", "minimized")}
def pixel(name, point):
    return images[name].getpixel(point)

# Outside window bounds: content-driven light must reach the wallpaper.
probe = (210, 320)
before, blue, green, minimized = (pixel(name, probe) for name in images)
assert blue[2] > before[2] + 12, (before, blue)
assert green[1] > blue[1] + 12 and green[2] < blue[2], (blue, green)
assert max(abs(a-b) for a, b in zip(before, minimized)) < 5, (before, minimized)
# Opaque application pixels stay sharp and unchanged by the glow.
assert pixel("before", (300, 250)) == pixel("after", (300, 250))
assert pixel("after", (1210, 500))[0] > pixel("before", (1210, 500))[0] + 12
print("PASS: coloured spill, live colour update, minimized cleanup, sharp app content")
for name in ("tuned", "zero"):
    images[name] = Image.open(out / f"{name}.png").convert("RGB")
delta = [a-b for a, b in zip(pixel("tuned", probe), before)]
assert min(delta) > 12 and max(delta) - min(delta) < 5, delta
far = (150, 320)
assert pixel("tuned", far)[2] > pixel("after", far)[2] + 5
assert pixel("zero", probe) == before
print("PASS: live radius, saturation and brightness controls")
