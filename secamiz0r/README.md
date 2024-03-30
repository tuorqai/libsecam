secamiz0r
=========

`secamiz0r` is a plugin for [frei0r](https://github.com/dyne/frei0r) that
utilizes `libsecam` to create _SECAM fire_ effect.

## Quick Start

### Windows

#### Get FFmpeg

* Download FFmpeg. Check out this link: https://www.ffmpeg.org/download.html. Keep in mind that it's a command line program. Make sure you're familiar with Command prompt.
* Unzip the downloaded FFmpeg somewhere. Open Command Prompt and navigate to the directory which contains `ffmpeg.exe`.
* Enter `ffmpeg -version` in the Command Prompt. Make sure the output contains `--enable-frei0r` string. If it's not present, then FFmpeg build you've downloaded is compiler without Frei0r support.

#### Get secamiz0r

* Download `libsecam-win32.zip` from the **Releases** tab.
* Unzip it somewhere. There should be `lib` folder which contains `secamiz0r.dll`.

#### Test if it works

* Open Command prompt.
* Enter `set FREI0R_PATH="<path to libsecam>/lib"`, e.g. if you've extracted `libsecam` to `C:/libsecam`, enter `set FREI0R_PATH="C:/libsecam/lib"`.
* Navigate to FFmpeg folder (where `ffmpeg.exe` resides).
* Enter `ffplay -f lavfi -i smptebars=640x480 -vf frei0r=secamiz0r`.
* If you see SMPTE bars with a nice SECAM fire effect, then it works.

#### Applying the effect to a video

* Basic command would be: `ffmpeg -i <path to video file> -vf scale=640:480,frei0r=secamiz0r -c:a copy -c:v libx264 -pix_fmt yuv420p <path to output file>`.
* Control the intensity: `frei0r=secamiz0r:0.15`, where `0.15` is the intensity value. This number can go from `0.0` up to `1.0`.

Things to consider:

* Resulting video will be quite noise, so the file will be *large*.
* The filter is *slow*. Powerful CPU is recommended.
* High resolutions are not recommended. Try to keep things at 480p and below.
