"""Model-independent tests of the resources of the FMU.

The data files of a model (src/models/<model>/resources/) become the folder
resources/ of the FMU (FMI 2.0, section 2.1.5). The tests extract the FMU to
a folder whose path contains a space and a non-ASCII character (see the
fixture unzipdir), so that the resource location is percent-encoded.
"""

import zipfile

RESOURCES = "resources/"


def test_resources_are_packaged(
    path_to_fmu, path_to_project_root, model_description
) -> None:
    """The FMU contains the model's resources, byte for byte, and nothing else
    below resources/. A model without resources gets no resources/ folder."""
    source = (
        path_to_project_root
        / "src"
        / "models"
        / model_description.modelName
        / "resources"
    )
    expected = (
        {
            f.relative_to(source).as_posix(): f.read_bytes()
            for f in source.rglob("*")
            if f.is_file()
        }
        if source.is_dir()
        else {}
    )

    with zipfile.ZipFile(path_to_fmu) as fmu:
        packaged = {
            name[len(RESOURCES) :]: fmu.read(name)
            for name in fmu.namelist()
            if name.startswith(RESOURCES) and not name.endswith("/")
        }

    assert packaged == expected


def test_fmu_is_tested_from_a_path_that_needs_encoding(unzipdir) -> None:
    """Guards the condition of the other tests: the path of the extracted FMU
    contains a space and a non-ASCII character."""
    assert " " in str(unzipdir)
    assert "ä" in str(unzipdir)


def test_resource_folder_is_announced(new_fmu, unzipdir) -> None:
    """The FMU understands the resource location of FMPy (a percent-encoded
    file URI) and logs the decoded folder."""
    log = []
    new_fmu(log=log)

    expected = (unzipdir / "resources").as_posix() + "/"
    assert any(f"Resource folder: '{expected}'" in message for _, _, message in log), (
        log
    )
