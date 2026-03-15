
import sys
import os
import shutil
import zipfile
import platform

def package_fmu(lib_path, model_desc_path, output_dir, fmu_name):
    """Package FMU according to FMI 2.0 standard."""
    
    # Determine architecture and platform
    machine = platform.machine().lower()
    arch = "aarch64" if machine in ("arm64", "aarch64") else "x86_64"

    system = platform.system()
    if system == "Windows":
        platform_name = f"{arch}-windows"
        lib_ext = ".dll"
    elif system == "Darwin":
        platform_name = f"{arch}-darwin"
        lib_ext = ".dylib"
    else:  # Linux
        platform_name = f"{arch}-linux"
        lib_ext = ".so"
    
    # Create temporary work directory
    work_dir = os.path.join(output_dir, "fmu_temp")
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(work_dir)
    
    try:
        # Create FMU directory structure
        binaries_dir = os.path.join(work_dir, "binaries", platform_name)
        os.makedirs(binaries_dir)
        
        # Copy library
        lib_file = lib_path if os.path.isfile(lib_path) else lib_path + lib_ext
        if not os.path.exists(lib_file):
            raise FileNotFoundError(f"Library not found: {lib_file}")
        shutil.copy2(lib_file, binaries_dir)
        
        # Copy modelDescription.xml to root
        if not os.path.exists(model_desc_path):
            raise FileNotFoundError(f"modelDescription.xml not found: {model_desc_path}")
        shutil.copy2(model_desc_path, work_dir)
        
        # Create zip file
        zip_path = os.path.join(output_dir, f"{fmu_name}.zip")
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(work_dir):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.relpath(file_path, work_dir)
                    zipf.write(file_path, arcname)
        
        # Rename to .fmu
        fmu_path = os.path.join(output_dir, f"{fmu_name}.fmu")
        if os.path.exists(fmu_path):
            os.remove(fmu_path)
        shutil.move(zip_path, fmu_path)
        
        print(f"Successfully created FMU: {fmu_path}")
        return 0
        
    except Exception as e:
        print(f"Error packaging FMU: {e}", file=sys.stderr)
        return 1
    finally:
        # Clean up temporary directory
        if os.path.exists(work_dir):
            shutil.rmtree(work_dir)

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: package_fmu.py <lib_path> <model_desc_path> <output_dir> <fmu_name>")
        sys.exit(1)
    
    sys.exit(package_fmu(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]))