# gen_autoDJ
An automatic DJ Winamp plugin, starting and stopping songs at configurable amplitude points.
* Just give it a big list of your favourites and it'll randomly play them whenever dead air approaches.
* Skips past quiet/silent starts, and ends the track before quiet/silent endings.
* Manually start other tracks, and it will work around you.
* Works with most audio formats, and karaoke tracks too.
* Works with [KaraokeManager](https://github.com/peeveen/karaokemanager) to allow requests to be queued up.

![AutoDJ](/media/autoDJScreenshot.png)

# Usage

- Download the latest release ZIP file and extract the contents.
- Edit the INI file with the paths of your playlist and requests files, and your preferred start/stop amplitudes.
- Copy the DLL and the INI to your Winamp plugins folder (usually `C:\Program Files (x86)\Winamp\Plugins`)
- Restart Winamp, and the AutoDJ window should appear.
- Note that it will not start cueing up tracks from your lists until you play something manually first.
- For best results, configure Winamp with a lengthy crossfade:

![Winamp](/media/winamp_fade.png)
