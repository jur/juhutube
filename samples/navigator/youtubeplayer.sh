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
	while [ true ]; do
		rm -f videocfg.cfg
		# Use navigator, so that the user can tell which video to play:
		echo "Starting navigator"
		if [ "$CATPAGETOKEN" != "" ]; then
			"$PROGRAM" -v videocfg.cfg -c "$CATPAGETOKEN" -i "$VIDEOID" -n "$CATNR" -m "$STATE" -t "$VIDPAGETOKEN"
		else
			"$PROGRAM" -v videocfg.cfg
		fi

		if [ -e videocfg.cfg ]; then
			# The user selected a video which should be played, so
			# get the information about it and play it:
			VIDPAGETOKEN=""
			source videocfg.cfg
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
