"""Check the actual flash image, including segment headers and alignment padding."""
Import("env")

def check_image(source, target, env):
    from pathlib import Path
    from SCons.Errors import BuildError
    image = Path(str(target[0]))
    limit = int(env.BoardConfig().get("upload.maximum_size"))
    actual = image.stat().st_size
    print(f"Final image: {actual:,} / {limit:,} bytes")
    if actual > limit:
        raise BuildError(errstr=f"Final binary exceeds application partition by {actual - limit} bytes")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_image)
