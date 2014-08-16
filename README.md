juhutube
========

Access YouTube from devices with limited resources

The library libjt provides support for the google youtube API.

The samples directoy contains example code. You need to edit the file
samples/include/clientid.h and add your client id and secret.
The following webpage describes how you get this:
https://developers.google.com/youtube/registering_an_application
You need to choose OAuth 2.0.

You can also download the JSON file from the Google Developer Console and
store it here: $HOME/.client_secret.json
Then you don't need to change clientid.h.

When you edited clientid.h, you can run:
make

The interface of the libjt is described in libjt/include/libjt.h as
doxygen comments.

STATUS: The library is still incomplete. There is no streaming support yet.

On Playstation 2 you should set the SDL video driver with:
export SDL_VIDEODRIVER=ps2gs

The default is fbcon, this also works with PS2, but is slower.

The library provides only access to the YoutTube Data API and not to the
YouTube streaming API. The YouTube streaming API needs to much resources (e.g.
a fully working web browser, Android or IOS installed). For streaming video you
need to have youtube-dl and mplayer installed.

http://rg3.github.io/youtube-dl/

YTNavigator
#########
YTNavigator is an example program for navigating through your subscribed
channels and play back videos. To use this program a YouTube account is
required.

You can navigate through the video playlists using the UP and DOWN keys.

You can navigate through the videos using the LEFT and RIGHT keys.

The current selected video is in the upper left corner.

To playback a video press the RETURN key. When you want to playback a playlist
starting with the current selected video, press SPACE. If you want to play the
videos in the reverse order press the R key.

To get all playlists of a channel press the P key.
