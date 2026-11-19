# Brooklyn Overlay Builder: step-by-step Windows guide

This guide helps you add subway map artwork to a blank Brooklyn Subway PCB design. Python prepares the image; EasyEDA Pro places it at the recorded coordinates and exports the color fabrication files. You then check the exported board before ordering.

**Included in the latest software ZIP:** the clean editable PCB project, its matching blank Gerber ZIP, the extension installer, launcher and calibration recipes. **The launcher downloads the PDF directly from MTA to your computer.** Install EasyEDA Pro and Python once, then use the simple route below.

**Tested:** native project import/reopen and complete live overlay exports using the reviewed September and October PDFs, EasyEDA Pro 3.2.149 and Brooklyn Overlay Builder 0.1.3. Other editor versions and the optional command-line route have not yet been tested live. Outputs require visual review before manufacture.

## Simplest Windows route

**Start here: [Download the latest Brooklyn Overlay Builder package (ZIP)](https://github.com/jgoldwein/BrooklynSubway/archive/refs/heads/main.zip).** Save it, then right-click it in File Explorer and choose **Extract All**. Extract into a **new folder** so you keep any earlier projects and results. Open the extracted folder containing **Start_Overlay.cmd**; if there is another `BrooklynSubway-main` folder inside, open that inner folder. Do not run the launcher from inside the ZIP.

**Everything we can supply is included:** `inputs/Blank_Map.eprj2`, `inputs/Blank_Gerbers.zip`, the ready-built extension, launcher and recipes. Do not move or rename them. The MTA PDF is downloaded locally and is not bundled in GitHub. See [template details and verification](../../hardware/builder/README.md).

Use this route:

1. Download and extract the **latest package** using the **Start here** link above. Use the newly extracted folder for all remaining steps.
2. Install [EasyEDA Pro](https://easyeda.com/page/download) and [Python 3.12 or newer](https://www.python.org/downloads/windows/) if needed. **Skip this step if both are already installed.** Pro 3.2.149 is the live-tested editor version; see [older official downloads](https://prodocs.easyeda.com/en/faq/client-version/).
3. Double-click **Start_Overlay.cmd**. If Windows shows **Open File - Security Warning** with **The publisher could not be verified**, follow [the warning instructions below](#windows-open-file-security-warning). The launcher sets up the Python packages on its first run, then **downloads the PDF directly from the [official MTA source](https://www.mta.info/map/5256)** to your computer. It checks the edition before preparing the board. If downloading fails, it explains the problem and opens a picker for a PDF you download yourself. Cancel the picker to stop. **No terminal commands, PDF renaming, recipe selection or output-folder naming are required for this route.**
4. Keep the command window open. **The launcher automatically opens this run's generated working copy in EasyEDA before you configure the extension**, using Windows' `.eprj2` file association. Accept an open-project prompt if EasyEDA shows one. In **Project Design**, double-click **Brooklyn MTA Active Map PCB** under **Blank-template-COPY > Brooklyn MTA Active Map** if the drawing is not already open. **You must see the PCB drawing, not just the Start Page, project tree or schematic.** If automatic opening fails, the command window prints **Open the working copy manually:** followed by the exact file path. Use that path, not an older project or the input template.
5. The launcher shows a **configure EasyEDA first** checklist. Leave it open while you finish setup. **With the PCB drawing visible**, choose **Advanced > Extensions Manager > Import** and select the included **tools > overlay > easyeda > Brooklyn-Overlay-Builder.eext**. Select its Installed entry, click **Config**, check **Allow interactive with external**, then close the manager. **Config/permission setup requires a loaded, visible PCB in the tested editor.** If it is missing or will not stay checked, close the manager, open the PCB drawing as described above, then return to Config. If already installed/configured, just verify the PCB is visible. **Only then return to the checklist and click OK.** Cancel stops this build session. The build timeout starts after you click OK, so setup time does not use it up. Do not restart the launcher after configuration.
6. After **SETUP COMPLETE**, choose **Advanced > Overlay Builder > Build map**, paste this run's complete local URL from the command window, and click **Confirm**. The URL is also saved in **Build-map-URL.txt** inside this run's **builds > Map_DATE_TIME** folder; its full path is printed. Open that text file if EasyEDA's background messages push the URL off screen. The command window also repeats the URL while waiting. Use only the URL for the current running session.
7. Wait for **Overlay complete** / **COMPLETE**, then follow [JLCPCB settings and color preview](#jlcpcb-settings-and-color-preview) below. The image disappears from the editor after export by design. Results are kept in **builds > Map_DATE_TIME > result** inside the extracted software folder.

The launcher recognizes the included September 22 and October 27, 2026 PDF fingerprints and chooses the corresponding recipe automatically. The October geometry was checked against the recovered reference and the cleaned template completed a live October export. Unknown/newer editions stop with a clear message instead of guessing a placement. The MTA PDF itself is intentionally not bundled.

Downloaded PDFs are kept under **builds > Map_DATE_TIME_source > Source_Map.pdf**. If the official link serves an unsupported edition, the launcher keeps that PDF and reports its fingerprint, then stops before opening a PCB. To use an existing local PDF directly, run `python tools/overlay/start.py --choose-pdf` or pass `--pdf "path-to-map.pdf"` instead.

The detailed file explanations below remain useful. The longer manual route is optional; do not perform both routes for the same build.

## Windows Open File security warning

When you double-click the downloaded **Start_Overlay.cmd**, Windows may show:

> Open File - Security Warning
>
> The publisher could not be verified. Are you sure you want to run this software?

The dialog lists **Publisher: Unknown Publisher** and **Type: Windows Command Script**. This project's launcher is not digitally signed, so this warning can appear again on later runs. It does not by itself mean the overlay build has failed.

1. Check that **Name** ends in **Start_Overlay.cmd** and its location is the new folder you extracted from this project's GitHub download.
2. If this is the project file you intended to run and you trust the source, click **Run** to continue. Otherwise click **Cancel**.
3. Leave **Always ask before opening this file** checked. You do not need to turn off Windows security features or run as administrator.
4. After clicking Run, the launcher command window should appear. Continue with the PCB/extension setup checklist when it opens.

These instructions describe this specific **Run / Cancel** dialog. A different warning should be reviewed separately.

## JLCPCB settings and color preview

Use these settings for this project's colorful printed map. The designer checked this path with the generated ZIP on November 19, 2026.

1. Open [JLCPCB](https://jlcpcb.com/) and its **Online PCB Quote** page. Click **Add gerber file**.
2. Select this run's **Gerbers-with-prepared-artwork.zip** from the printed output path. Keep the ZIP compressed. Wait for processing.
3. Under **PCB Specifications**, set **PCB Color → White**.
4. Set **Surface Finish → ENIG**.
5. Expand **Advanced Options**. Under **Silkscreen Technology**, select **EasyEDA multi-color silkscreen** (also called **EasyEDA MultiColor Silkscreen**).
6. Above the uploaded board thumbnails, click **Gerber Viewer**. Select its **2D** tab to inspect the full-color front of the board. Also inspect the back.
7. Zoom in and check the map against the LED pads at the top, middle, bottom and edges. Confirm the lower-left Wi-Fi QR code, **BrooklynMap-Avi** / **192.168.4.1** labels, OLED area and button labels are present and readable. Check that unintended personal markings are absent.

**Expected result:** the viewer shows the colored subway map on a white board, including the restored setup QR code. The designer's generated-file preview showed this result. This visual check does not replace electrical review or the manufacturer's production checks.

You can inspect the preview without placing or paying for an order. The price and quantity shown by the quote page are not part of these instructions. Confirm the remaining board specifications against your intended fabrication requirements before ordering.

Do not look for **Gerber View** in EasyEDA's Export menu; that menu does not contain this viewer in the tested version. Use the **Gerber Viewer** link on JLCPCB's uploaded-file quote page. See also [JLCPCB's official colorful-silkscreen instructions](https://jlcpcb.com/help/article/how-to-design-multi-color-silkscreen-using-easyeda).

## First, understand the files

**What is the helper?** It is the included **Python helper tool**, [automate.py](automate.py), which runs on your computer. It checks your input files, prepares the map image, creates a working PCB copy and packages the final Gerber ZIP. It also supplies the local job address that connects it to the EasyEDA extension. It is not a person, an AI service or a separate download: it comes in the software ZIP below. You start it in [step 8](#8-start-the-helper-and-create-your-working-pcb-copy).

There are **three different ZIP files** in this workflow. They serve different purposes:

- **[BrooklynSubway-main.zip](https://github.com/jgoldwein/BrooklynSubway/archive/refs/heads/main.zip) - the software download.** GitHub creates this ZIP from the [repository](https://github.com/jgoldwein/BrooklynSubway). It contains the [overlay tools](https://github.com/jgoldwein/BrooklynSubway/tree/main/tools/overlay), instructions, firmware source and other project files. You extract this ZIP so you can run the overlay tools; [step 3](#3-download-the-software-tools-from-github) explains how. It is not a PCB fabrication file and does not go to the PCB manufacturer.
- **Your blank Gerber ZIP - the starting fabrication files.** This describes the board before the map is added: copper, holes, masks, printed labels and other fabrication layers. The Python helper tool uses it as the reference, or **baseline**, whose protected contents must remain unchanged. Keep this ZIP compressed. See [input requirements](#1-check-that-you-have-the-three-build-inputs) and [where to put it](#4-put-copies-of-your-inputs-in-one-place). The matching ZIP is already included at `inputs/Blank_Gerbers.zip`; [download that file separately only if needed](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Gerbers.zip). The historical art-free fabrication derivative is not a substitute.
- **[Gerbers-with-prepared-artwork.zip - the finished result](#11-find-and-check-your-finished-fabrication-zip).** The Python helper tool creates this after EasyEDA exports the artwork. This is the ZIP you preview and, after checking it, may submit to the manufacturer. Keep it compressed. This link explains where to find your locally generated result; there is no finished map ZIP to download from this guide.

You also need these files:

- **[Blank_Map.eprj2](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Map.eprj2):** the editable [EasyEDA Pro](#2-install-easyeda-pro) source, including the PCB and attached schematics. EasyEDA needs this to insert the image. It must match your blank Gerber ZIP, including the labels. A Gerber ZIP cannot replace this file. The file is already included in `inputs`; this link downloads the actual project if needed.
- **[The MTA `.pdf`](https://www.mta.info/map/5256):** the source subway map downloaded or otherwise obtained from the MTA. The PDF edition must match the selected recipe. You provide this locally; it is not bundled with the tools. The MTA link may serve a newer edition; read the [edition requirement](#1-check-that-you-have-the-three-build-inputs) before proceeding.
- **[A recipe `.json`](recipes/brooklyn-september-2026.json):** a small text file supplied with the tools. It records the approved PDF's fingerprint, crop rectangle, size and placement. It contains calibration numbers, not map artwork. Do not edit it to bypass a PDF mismatch. The link opens the September 2026 recipe for reference; the [October recipe](recipes/brooklyn-october-2026.json) is also included and the launcher chooses automatically; it is already present in your software download.
- **[Brooklyn-Overlay-Builder.eext](#6-prepare-python-and-locate-the-included-installer):** the EasyEDA extension installer. It is already included in the software download at `tools/overlay/easyeda/Brooklyn-Overlay-Builder.eext`; [import it into EasyEDA in step 7](#7-install-the-extension-in-easyeda-pro). It adds **Advanced > Overlay Builder > Build map**. It is not the board project. You can also [download the ready-built installer directly](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/tools/overlay/easyeda/Brooklyn-Overlay-Builder.eext).

**Files you do not have yet:** [Blank-template-COPY.eprj2 and the prepared image](#8-start-the-helper-and-create-your-working-pcb-copy), and the [final Gerber ZIP](#11-find-and-check-your-finished-fabrication-zip), are generated later. Do not search for them inside the software download.

<details>
<summary><strong>Detailed manual setup and troubleshooting (optional)</strong></summary>

## 1. Check that you have the three build inputs

Before installing the tools, identify:

1. **[Download Blank_Map.eprj2](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Map.eprj2)** - the cleaned editable PCB and attached schematics. **Already included** in the latest software ZIP under `inputs`.
2. **[Download Blank_Gerbers.zip](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Gerbers.zip)** - the verified matching baseline. **Already included** under `inputs`; keep it compressed. The historical art-free manufacturing ZIP is a different revision and cannot substitute for it.
3. The **[source map `.pdf` from the MTA](https://www.mta.info/map/5256)**, matching the **[reviewed recipe](recipes/brooklyn-september-2026.json)**. The MTA link may serve a newer edition that the included recipes do not accept; it is not a guaranteed download of a supported edition. The launcher also recognizes the October 27, 2026 edition.

If using the latest software ZIP, the first two files are already present; do not download or copy them again. If using an older download, obtain a fresh software ZIP or use the direct file links above. You must obtain the PDF yourself.

The first two links above download the actual files, not explanatory pages. The MTA link opens the official PDF. The [verification record](../../hardware/builder/verification.json) documents the tested project pair.

The included recipes recognize September 22 and October 27, 2026 PDFs. The launcher chooses by fingerprint automatically. The manual command below explicitly uses the October recipe; using September manually requires selecting its recipe with `--recipe`. The [official MTA map page](https://www.mta.info/map/5256) can change editions; an unrecognized edition is rejected.

## 2. Install EasyEDA Pro

1. Open the [official EasyEDA Download Center](https://easyeda.com/page/download).
2. Choose **EasyEDA Pro Edition**, then the installer for your operating system. Do not choose EasyEDA Standard.
3. Run the installer and complete its prompts.
4. Open EasyEDA Pro and complete its first-run setup. Leave it at the Start Page.

The verified Windows walkthrough uses **Pro 3.2.149**. That version is available from [EasyEDA's official older-client downloads](https://prodocs.easyeda.com/en/faq/client-version/). Newer versions may have different menus. If the tested version is already installed, skip reinstalling it.

**Expected result:** EasyEDA Pro opens. Do not open a board or the Build map dialog yet.

## 3. Download the software tools from GitHub

**What are you downloading, and why?** The following link downloads the repository as `BrooklynSubway-main.zip`. It gives your computer a local copy of `tools/overlay`, including the Python helper and extension source. Reading this guide in a browser does not install those files.

1. Click **[Download BrooklynSubway-main.zip](https://github.com/jgoldwein/BrooklynSubway/archive/refs/heads/main.zip)**.
2. Save it in your Windows **Downloads** folder. This is the software ZIP, not the blank Gerber ZIP.
3. Open **File Explorer > Downloads**.
4. Right-click `BrooklynSubway-main.zip` and select **Extract All**. Accept the suggested destination.
5. Open the extracted folder. If it contains another `BrooklynSubway-main` folder, open that inner folder.
6. Find both **`README.md`** and the **`tools`** folder in the same window. This is the **repository folder** used throughout the remaining steps.

The full download includes firmware and documentation because it is a copy of the repository. **You do not need PlatformIO, and you do not need to program or connect a PCB to build the overlay.** You will use the `tools/overlay` files.

For reference, the [overlay code directory](https://github.com/jgoldwein/BrooklynSubway/tree/main/tools/overlay) lets you browse those files online; browsing it does not download the complete tool to your computer.

**Expected result:** an ordinary extracted folder containing `README.md` and `tools`. Do not work inside File Explorer's view of the ZIP itself.

## 4. Put copies of your inputs in one place

Inside the [repository folder you extracted in step 3](#3-download-the-software-tools-from-github):

1. Open the included **[inputs folder](../../inputs/README.md)**. It already comes in the latest repository ZIP; you do not need to create it. It already contains `Blank_Map.eprj2` and `Blank_Gerbers.zip`; its `README.md` explains them. If you downloaded an older ZIP without this folder, create a folder named `inputs` beside `README.md`.
2. **Skip this step with the latest ZIP:** `inputs/Blank_Map.eprj2` is already supplied. If restoring a missing file individually, [download it here](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Map.eprj2) into `inputs` without changing its filename.
3. **Skip this step with the latest ZIP:** `inputs/Blank_Gerbers.zip` is already supplied. If restoring a missing file individually, [download it here](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Gerbers.zip) into `inputs`. Do not extract it or substitute an older manufacturing archive.
4. Copy your reviewed PDF from the [official MTA map source](https://www.mta.info/map/5256) into `inputs`. Rename this copy **`Source_Map.pdf`**. Check the [required PDF edition](#1-check-that-you-have-the-three-build-inputs) first: the currently served MTA PDF may differ from the [included recipes](recipes/).

Renaming the copies simplifies the [build command in step 8](#8-start-the-helper-and-create-your-working-pcb-copy). It does not change the contents or make a mismatched file valid. Keep your original files elsewhere unchanged. If Windows hides file extensions, turn on **File name extensions** in File Explorer so you do not accidentally produce a name such as `Source_Map.pdf.pdf`.

Your repository folder should now contain:

```text
BrooklynSubway-main/
  README.md
  tools/
    overlay/
      automate.py
      build_extension.py
      requirements.txt
      recipes/
        brooklyn-september-2026.json
  inputs/
    Blank_Map.eprj2
    Blank_Gerbers.zip
    Source_Map.pdf
```

**Expected result:** the three files above exist in `inputs`. The project contains no large map image, and its Gerber ZIP matches its labels and circuitry.

## 5. Install Python and open PowerShell in the right folder

Python runs the helper scripts; PowerShell is the window where you enter commands and read their results.

1. If needed, install Python **3.12 or newer** from the [official Python Windows downloads](https://www.python.org/downloads/windows/). Enable the installer's option to add Python to PATH if offered. Python 3.13 was used for testing.
2. In File Explorer, return to the repository folder containing `README.md`, `tools` and `inputs`.
3. Click the address bar at the top of File Explorer.
4. Type `powershell` and press **Enter**. This opens PowerShell in that folder.
5. In PowerShell, enter:

```powershell
python --version
```

**Expected result:** a Python version of 3.12 or newer. If Python is not found or Windows offers to open a store instead, finish the Python installation before continuing.

Confirm the folder is correct:

```powershell
Test-Path .\tools\overlay\automate.py
```

**Expected result:** `True`. If it says `False`, you opened PowerShell in the wrong folder. Return to step 3's folder check.

Keep this PowerShell window open. Run the remaining commands there, not in EasyEDA or its file picker.

## 6. Prepare Python and locate the included installer

Run each command separately. Wait for it to finish before entering the next.

First, create a private Python environment for these tools:

```powershell
python -m venv .venv-overlay
```

This creates `.venv-overlay` inside the repository folder. It keeps the helper's Python packages separate from other projects. It may finish without printing a message.

Next, install the image/PDF packages listed in `requirements.txt`:

```powershell
.\.venv-overlay\Scripts\python -m pip install -r tools/overlay/requirements.txt
```

`requirements.txt` is the supplied list of package versions. Wait for installation to finish without errors.

**The extension installer is already built. You do not need another command.** In File Explorer, open the repository folder, then **tools > overlay > easyeda**. The file is **[Brooklyn-Overlay-Builder.eext](easyeda/Brooklyn-Overlay-Builder.eext)**. You will select it in EasyEDA in the next step.

You can also [download the installer directly](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/tools/overlay/easyeda/Brooklyn-Overlay-Builder.eext). Downloading it separately is unnecessary if you extracted the latest software ZIP.

**Expected result:** the Python package installation finished without errors, and the included installer is visible. No map or copied PCB has been created yet.

## 7. Install the extension in EasyEDA Pro

**Before opening Extensions Manager, load a PCB drawing.** For this manual route, use **File > Open Project** to open the included `inputs/Blank_Map.eprj2`, then in **Project Design** double-click **Brooklyn MTA Active Map PCB**. Leave the board drawing visible. Do not edit the input template. The Config/permission setting does not work from the Start Page alone in the tested editor. Later steps create and open a separate working copy for the actual build. The simpler launcher route above automatically opens the working project first instead.

1. In EasyEDA, choose **Advanced > Extensions Manager**.
2. Click **Import** at the bottom left.
3. Select `Brooklyn-Overlay-Builder.eext` inside the repository folder under **tools > overlay > easyeda**. Accept replacement if you are updating an existing installation.
4. Select the **Installed** tab.
5. Click the **Brooklyn Overlay Builder** name in the left list. Its details should appear on the right.
6. Verify the version is **0.1.3 or newer**. In the tested version it appears on a version button near the top of the details.
7. Click **Config**, immediately beside **Detail**.
8. Check **Allow interactive with external**. This lets the extension communicate with the helper on your computer. Leave the other settings unchanged.
9. Click **Close**. Restart EasyEDA if it requests a restart.

**Expected result:** the extension is installed and its external-interaction permission is checked. Do not start Build map yet. Installing the extension does not prepare the image or create the copied PCB.

## 8. Start the helper and create your working PCB copy

Return to the PowerShell window from step 5. With the input filenames from step 4, paste this single command:

```powershell
.\.venv-overlay\Scripts\python tools/overlay/automate.py --project "inputs\Blank_Map.eprj2" --baseline "inputs\Blank_Gerbers.zip" --pdf "inputs\Source_Map.pdf" --recipe "tools/overlay/recipes/brooklyn-october-2026.json" --output "Map_Build_01"
```

The options mean:

- **`--project`:** the editable blank EasyEDA source to copy.
- **`--baseline`:** the matching blank fabrication ZIP whose protected contents must be preserved.
- **`--pdf`:** the local MTA PDF to check, crop and resize.
- **`--output`:** a new folder for this build's copies, prepared image and results. The helper creates it inside the repository folder.

**Expected result:** PowerShell prints **Prepared the map. Original files are unchanged.**, then **Open:** and an absolute path to `Blank-template-COPY.eprj2`. It also prints a local job URL starting with `http://127.0.0.1:`.

Do not continue if it prints **ERROR**. Keep the error message for troubleshooting.

**Leave PowerShell open.** It is running a local helper and waiting for EasyEDA, for up to 20 minutes. Not returning to a prompt at this point is expected.

The newly created files include:

- **`Map_Build_01/Blank-template-COPY.eprj2`:** the working copy you open in EasyEDA. Your original input is unchanged.
- **`Map_Build_01/prepared/prepared-map.png`:** the cropped and resized image. The extension places it automatically; you do not drag it manually.
- **`Map_Build_01/prepared/manifest.json`:** the record of source/image fingerprints and placement.

## 9. Open the generated PCB document

1. In EasyEDA's Start Page, click **Open Project**, the yellow folder under Quick Start.
2. In the file picker's **File name** field, paste the complete path printed after **Open:** in PowerShell. Click **Open**.
3. Click the vertical **Project Design** sidebar tab on the left.
4. Expand **Blank-template-COPY > Brooklyn MTA Active Map**.
5. Double-click **Brooklyn MTA Active Map PCB**, the green board-icon entry below **Brooklyn MTA Active Map Schematics**.
6. Confirm the board drawing is visible and the project tree's top-level name is **Blank-template-COPY**. Do not select a similarly named older project from Recent Design.

**Expected result:** the blank PCB drawing is open. The sidebar is named **Project Design**; the document inside it is named **Brooklyn MTA Active Map PCB**. A schematic or 3D preview is not the active PCB editor. A renamed template may use different internal document names.

## 10. Connect EasyEDA to this build

1. With the PCB drawing visible, choose **Advanced > Overlay Builder > Build map**.
2. Return to PowerShell and copy the **entire local job URL** beginning `http://127.0.0.1:`.
3. Paste it into the empty field in EasyEDA's **Build subway map** dialog.
4. Click **Confirm** and wait for the export to finish.

The URL identifies the helper running on **your own computer**. It includes a port number and a long random token. Use the actual URL from this run, not one copied from a screenshot or an earlier build. Do not enter a filename, GitHub link, MTA link or `192.168.4.1`.

The extension first exports the blank PCB and compares it with your baseline. It stops before inserting the map if they do not match. Otherwise it inserts the image, exports again and removes the temporary image.

**Expected result:** EasyEDA shows **Overlay complete**, and PowerShell prints **COMPLETE:** followed by the final ZIP path.

**The graphic appears briefly, then disappears from the editor. That is expected.** The extension removes its temporary image after export to leave the template map-less. The completed artwork is in the output ZIP, not left in your open PCB.

## 11. Find and check your finished fabrication ZIP

1. In File Explorer, open the repository folder, then **Map_Build_01 > result**.
2. Find **`Gerbers-with-prepared-artwork.zip`**. This is the finished fabrication archive, not the GitHub software download or input baseline.
3. Find **`integrity-report.json`** beside it. Open it in a text editor to read the comparison record. `protectedPayloadsByteIdentical: true` means the output's non-color files were copied exactly from your selected baseline.
4. Follow [JLCPCB settings and color preview](#jlcpcb-settings-and-color-preview): **White** PCB, **ENIG** finish, and **Advanced Options → Silkscreen Technology → EasyEDA multi-color silkscreen**, then open the uploaded-file **Gerber Viewer**. A copper-only viewer cannot show the color map.
5. Check LED/station alignment at the top, center, bottom and far edges. Check the QR code, labels and both PCB sides for unwanted personal text or artwork.

A passing integrity report does not prove that the chosen starting circuit is correct or approve map alignment, artwork permissions or manufacture. Review those separately before placing an order.

## Make another build later

Keep the installed extension and Python environment. Repeat steps 8-11 with a new output folder, such as `--output "Map_Build_02"`. Existing result folders are never overwritten, including folders left by failed attempts. Each new run supplies a new local job URL.

## Troubleshooting

- **No `Blank-template-COPY.eprj2`:** step 8 has not finished successfully. The file is not included in the downloaded software ZIP.
- **Cannot find the extension installer:** open **tools > overlay > easyeda** inside the extracted repository folder, or use the direct installer download in step 6.
- **Extensions Manager closes when selecting the extension:** download/import a current package. Versions before 0.1.2 omitted metadata required by EasyEDA 3.2's details panel.
- **External interaction permission error / Config not available:** first close Extensions Manager and open the PCB drawing (Project Design > Brooklyn MTA Active Map PCB). With the drawing visible, return to Advanced > Extensions Manager > Installed > Brooklyn Overlay Builder > Config > Allow interactive with external.
- **No Overlay Builder menu:** activate the PCB drawing and look under **Advanced**. Check that the extension is Enabled.
- **Nothing happens:** inspect EasyEDA's bottom **Log** tab for `[Overlay Builder]` entries. Check the version printed there, not just the installer filename. Versions 0.1.1 and newer show failures in a dialog and Log.
- **Cannot reach helper:** keep PowerShell open and use the current complete job URL. A failed or expired helper must be restarted with a fresh output folder.
- **EasyEDA error messages hide the URL:** open this run's **Build-map-URL.txt** in its build folder. The helper prints its exact path after setup confirmation and repeats the URL while waiting. Messages such as `ECONNRESET` and `device not found` come from EasyEDA; check whether its PCB drawing loads before proceeding. They do not by themselves establish that the overlay export failed.
- **PCB does not match blank Gerber:** confirm the correct generated PCB is active and its labels/circuit match the baseline. A similarly named file is not sufficient. Do not bypass the check.
- **Same PCB rejected after restoring the QR code:** use the latest package. EasyEDA can reorder the QR's filled regions or trace equivalent polygon outlines. The helper compares the exact filled geometry of supported axis-aligned silkscreen regions, without rounding or rasterization. Changed shapes still fail; copper and other fabrication layers retain their existing checks. This correction was verified against the November 19 fresh-walkthrough export that triggered the false mismatch.
- **Map-size image already present:** start from the genuinely map-less template. The extension does not guess which image to delete.
- **PDF fingerprint/hash mismatch:** the PDF is a different edition. Obtain the reviewed matching edition or ask the maintainer to validate a new recipe. Renaming the PDF cannot resolve this.
- **Missing editor API:** report the EasyEDA version and message. Other clients are not yet validated; the [manual import workflow](README.md#quick-path-prepare-the-recovered-brooklyn-crop) is available.

</details>

## Advanced details

The helper uses standard Python plus the pinned packages in `requirements.txt`; the extension has no third-party JavaScript dependencies. It binds to `127.0.0.1` only and uses an unpredictable per-run URL token. Keep that URL private.

The helper can attempt downloading the recipe's source by omitting `--pdf`. The MTA server returned HTTP 403 in local tests, and its URL can change editions; the local-PDF workflow above is the verified route. Unknown editions are rejected.

The extension uses mm-to-mil conversion and explicit canvas/data-origin conversion. Version 0.1.3 requests millimeters, 4:5 coordinates and the standard layers including drill drawings to match this template. It never calls save on the source project. The final package uses the four color-fabrication payloads from EasyEDA's mapped export; all other payloads come byte-for-byte from the baseline. The two known English/Chinese ordering-note filenames are informational in comparison; the baseline's note is preserved.

An optional `--easyeda-cli` adapter targets the [documented EasyEDA 4.1.60+ CLI](https://prodocs.easyeda.com/en/api/guide/cli.html). It is simulated in tests, not live-validated. EasyEDA 3.2.149 cannot use it. This beginner walkthrough uses the tested extension route instead.

Tests: `python tools/overlay/test_start.py`, `python tools/overlay/test_overlay.py`, `python tools/overlay/test_automate.py` and `node tools/overlay/test_extension.cjs`.

Official API references: [image creation](https://prodocs.easyeda.com/en/api/reference/pro-api.pcb_primitiveobject.create.html), [origin conversion](https://prodocs.easyeda.com/en/api/reference/pro-api.pcb_document.convertcanvasorigintodataorigin.html), [Gerber export](https://prodocs.easyeda.com/en/api/reference/pro-api.pcb_manufacturedata.getgerberfile.html) and [extension installation](https://prodocs.easyeda.com/en/api/guide/how-to-start.html).
