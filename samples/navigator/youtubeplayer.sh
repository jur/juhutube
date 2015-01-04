#!/bin/ash
#set -x
DEFAULTPREFIX="$HOME"
DEFAULTSHAREDIR="$DEFAULTPREFIX/share/ytnavigator"
SHAREDIR="$2"
if [ -z "$SHAREDIR" ]; then
	SHAREDIR="$DEFAULTSHAREDIR"
fi
DEFAULTPROGRAM="$DEFAULTPREFIX/bin/ytnavigator"
PROGRAM="$1"
if [ -z "$PROGRAM" ]; then
	PROGRAM="$DEFAULTPROGRAM"
fi
echo "      Initializing..."
AGENT="$(youtube-dl --dump-user-agent)"
which ffplay >/dev/null
if [ $? -eq 0 ]; then
	PLAYER="ffplay -fs -autoexit -"
else
	if [ "$DISPLAY" = "" ]; then
		PLAYER="mplayer -vo sdl -ao sdl -cache 1024 -hardframedrop -"
	else
		PLAYER="mplayer -cache 1024 -"
	fi
fi

checkforcross()
{
	# Wait until the CROSS button is pressed.
	KILLPID=$1
	while sleep 1; do
		BUTTONS="$(head -n 2 /proc/ps2pad | tail -n 1 | cut -b 32-35)"
		if [ "$BUTTONS" = "FFDF" ]; then
			kill $KILLPID 2>/dev/null
			break
		fi
	done
}

# Prefer wget, because it has less problems than curl.
which wget >/dev/null
USE_WGET=$?

if [ -e "/dev/ps2gs" ]; then
	# Playstation 2 video driver is available
	# Use it, because it is faster:
	export SDL_VIDEODRIVER="ps2gs"
	# Some video need more than one YUV overlay which is not supported by
	# ps2gs, so disable it.
	export SDL_VIDEO_YUV_HWACCEL="0"
fi

if [ -x "$PROGRAM" ]; then
	RETVAL=""
	while [ true ]; do
		CFG="$(mktemp -t ytplayXXXXXXXXX)"
		rm -f "$CFG"
		# Use navigator, so that the user can tell which video to play:
		echo "      Starting navigator"
		if [ "$RETVAL" != "" ]; then
			"$PROGRAM" -f -o "$SHAREDIR" -v "$CFG" -p "$PLAYLISTID" -k "$CATPAGETOKEN" -i "$VIDEOID" -n "$CATNR" -j "$CHANNELSTART" -m "$STATE" -t "$VIDPAGETOKEN" -u "$VIDNR" -r "$RETVAL" -c "$CHANNELID" -e "$SELECTEDMENU"
			RETVAL="$?"
		else
			"$PROGRAM" -f -o "$SHAREDIR" -v "$CFG"
			RETVAL="$?"
		fi

		if [ "$RETVAL" = "4" ]; then
			if [ "$(whoami)" = "root" ]; then
				halt
			else
				sudo halt
			fi
			break
		fi

		if [ "$RETVAL" != "0" -a -e "$CFG" ]; then
			# The user selected a video which should be played, so
			# get the information about it and play it:
			VIDPAGETOKEN=""
			source "$CFG"
			echo "      Selected video:"
			echo
			echo "      $VIDEOTITLE"
			echo
			echo "      Getting URL..."
			echo
			if [ -e "/proc/ps2pad" ]; then
				echo "      Hold O for 1 second to cancel"
				echo
			fi
			URLFILE="/tmp/url.$$"
			rm -f "$URLFILE"
			youtube-dl -g -f 5 --cookies=/tmp/ytcookie-$VIDEOID.txt https://www.youtube.com/watch?v=$VIDEOID >"$URLFILE" &
			YDLPID=$!
			if [ -e "/proc/ps2pad" ]; then
				checkforcross $YDLPID &
				CHECKPID=$!
			fi
			wait $YDLPID
			URL="$(cat $URLFILE)"
			rm -f "$URLFILE"
			if [ -e "/proc/ps2pad" ]; then
				kill $CHECKPID 2>/dev/null
			else
				echo
			fi
			if [ $? -eq 0 ]; then
				echo "      Starting player..."
				echo
				if [ $USE_WGET -ne 0 ]; then
					CURL_PARAM=
					if [ "$SSL_CERT_PATH" != "" ]; then
						CURL_PARAM="--capath $SSL_CERT_PATH"
					fi
					curl $CURL_PARAM --user-agent "$AGENT" --cookie "/tmp/ytcookie-${VIDEOID}.txt" "$URL" | $PLAYER
				else
					WGET_PARAM=
					if [ "$SSL_CERT_PATH" != "" ]; then
						WGET_PARAM="--ca-directory=$SSL_CERT_PATH"
					fi
					wget $WGET_PARAM --user-agent="$AGENT" -o /dev/null -O - --load-cookies /tmp/ytcookie-${VIDEOID}.txt - "$URL" | $PLAYER
				fi
			else
				echo "      Failed to get URL."
			fi
			rm -f "/tmp/ytcookie-${VIDEOID}.txt"
		else
			echo "      Stopped with $RETVAL ($CFG)"
			break
		fi
	done
fi
