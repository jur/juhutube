juhutube
========

Access YouTube from devices with limited resources

The library libjt provides support for the google youtube API.

The samples directoy contains example code. You need to edit the file
samples/include/clientid.h and add your client id and secret.
The following webpage describes how you get this:
https://developers.google.com/youtube/registering_an_application
You need to choose OAuth 2.0.

When you edited clientid.h, you can run:
make

The interface of the libjt is described in libjt/include/libjt.h as
doxygen comments.

STATUS: The library is still incomplete. There is no streaming support yet.
