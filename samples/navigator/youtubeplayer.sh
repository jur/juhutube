#!/bin/ash
#set -x
DEFAULTPROGRAM="/usr/bin/ytnavigator"
PROGRAM="$1"
if [ -z "$PROGRAM" ]; then
	PROGRAM="$DEFAULTPROGRAM"
fi
echo "Initializing..."
AGENT="$(youtube-dl --dump-user-agent)"
which ffplay >/dev/null
if [ $? -eq 0 ]; then
	PLAYER="ffplay -autoexit -"
else
	if [ "$DISPLAY" = "" ]; then
		PLAYER="mplayer -vo sdl -ao sdl -cache 1024 -hardframedrop -"
	else
		PLAYER="mplayer -cache 1024 -"
	fi
fi

# Prefer wget, because it has less problems than curl.
which wget >/dev/null
USE_WGET=$?

if [ -e "/dev/ps2gs" ]; then
	# Playstation 2 video driver is available
	# Use it, because it is faster:
	export SDL_VIDEODRIVER="ps2gs"
fi

if [ -x "$PROGRAM" ]; then
	RETVAL=""
	while [ true ]; do
		CFG="$(mktemp -t ytplayXXXXXXXXX)"
		rm -f "$CFG"
		# Use navigator, so that the user can tell which video to play:
		echo "Starting navigator"
		if [ "$RETVAL" != "" ]; then
			"$PROGRAM" -v "$CFG" -p "$PLAYLISTID" -k "$CATPAGETOKEN" -i "$VIDEOID" -n "$CATNR" -j "$CHANNELSTART" -m "$STATE" -t "$VIDPAGETOKEN" -u "$VIDNR" -r "$RETVAL" -c "$CHANNELID"
			RETVAL="$?"
		else
			"$PROGRAM" -v "$CFG"
			RETVAL="$?"
		fi

		if [ "$RETVAL" != "0" -a -e "$CFG" ]; then
			# The user selected a video which should be played, so
			# get the information about it and play it:
			VIDPAGETOKEN=""
			source "$CFG"
			echo "Selected video $VIDEOTITLE"
			echo "Getting URL..."
			URL="$(youtube-dl -g -f 5 --cookies=/tmp/ytcookie-$VIDEOID.txt http://www.youtube.com/watch?v=$VIDEOID)"
			echo "Starting player..."
			if [ $USE_WGET -ne 0 ]; then
				curl --user-agent "$AGENT" --cookie "/tmp/ytcookie-${VIDEOID}.txt" "$URL" | $PLAYER
			else
				wget --user-agent="$AGENT" -o /dev/null -O - --load-cookies /tmp/ytcookie-${VIDEOID}.txt - "$URL" | $PLAYER
			fi
			rm "/tmp/ytcookie-${VIDEOID}.txt"
		else
			echo "Stopped with $RETVAL ($CFG)"
			break
		fi
	done
fi
