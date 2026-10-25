# Junta as duas telas do DS (cima 3D, baixo interface) numa imagem ampliada 2x
import glob
from PIL import Image
for top in sorted(glob.glob("ds*_top.ppm")):
    base = top[:-8]
    t = Image.open(top).convert("RGB")
    b = Image.open(base + "_bottom.ppm").convert("RGB")
    out = Image.new("RGB", (256, 192 * 2 + 8), (40, 40, 46))
    out.paste(t, (0, 0)); out.paste(b, (0, 200))
    out.resize((512, (192 * 2 + 8) * 2), Image.NEAREST).save(base + ".png")
    print(base)
