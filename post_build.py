import os
import shutil
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

def after_build(source, target, env):
    firmware_path = env.subst("$BUILD_DIR/firmware.bin")
    output_dir = os.path.join(env["PROJECT_DIR"], "fw_update")
    firmware_version = "1.0.0"
    target_filename = "hackffmbadgev1_fw.bin"

    # Ensure the output directory exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Copy the firmware file
    if os.path.isfile(firmware_path):
        shutil.copy(firmware_path, os.path.join(output_dir, target_filename))
        print(f"✅ Firmware copied to: {os.path.join(output_dir, target_filename)}")
    else:
        print(f"⚠️  Firmware file not found: {firmware_path}")

    # Write firmware version to text file
    with open(os.path.join(output_dir, "hackffmbadgev1_vers.txt"), "w") as f:
        f.write(firmware_version)

# Register this function to run after firmware.bin is built
env.AddPostAction(env.subst("$BUILD_DIR/firmware.bin"), after_build)
