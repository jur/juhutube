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
