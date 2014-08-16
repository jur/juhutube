#!/bin/bash
DEFAULTPROGRAM="/usr/local/bin/ytnavigator"
PROGRAM="$1"
if [ -z "$PROGRAM" ]; then
	PROGRAM="$DEFAULTPROGRAM"
fi
echo "Initializing..."
AGENT="$(youtube-dl --dump-user-agent)"

if [ -e "/dev/ps2gs" ]; then
	# Playstation 2 video driver is available
	# Use it, because it is faster:
	export SDL_VIDEODRIVER="ps2gs"
fi

if [ -x "$PROGRAM" ]; then
	RETVAL=""
	#set -x
	while [ true ]; do
		CFG="$(mktemp /tmp/youtubeplayerXXXXXXXXX.cfg)"
		rm -f "$CFG"
		# Use navigator, so that the user can tell which video to play:
		echo "Starting navigator"
		if [ "$RETVAL" != "" ]; then
			"$PROGRAM" -v "$CFG" -p "$PLAYLISTID" -c "$CATPAGETOKEN" -i "$VIDEOID" -n "$CATNR" -m "$STATE" -t "$VIDPAGETOKEN" -u "$VIDNR" -r "$RETVAL"
		else
			"$PROGRAM" -v "$CFG"
		fi
		RETVAL="$?"

		if [ "$RETVAL" != "0" -a -e "$CFG" ]; then
			# The user selected a video which should be played, so
			# get the information about it and play it:
			VIDPAGETOKEN=""
			source "$CFG"
			echo "Selected video $VIDEOTITLE"
			echo "Getting URL..."
			URL="$(youtube-dl -g -f 5 --cookies=/tmp/ytcookie-$VIDEOID.txt http://www.youtube.com/watch?v=$VIDEOID)"
			echo "Starting player..."
			if [ "$DISPLAY" = "" ]; then
				# No X11 use SDL:
				wget --user-agent="$AGENT" -o /dev/null -O - --load-cookies /tmp/ytcookie-${VIDEOID}.txt - "$URL" | mplayer -vo sdl -ao sdl -cache 1024 -framedrop -
			else
				wget --user-agent="$AGENT" -o /dev/null -O - --load-cookies /tmp/ytcookie-${VIDEOID}.txt - "$URL" | mplayer -cache 1024 -
			fi
			rm "/tmp/ytcookie-${VIDEOID}.txt"
		else
			break
		fi
	done
fi
