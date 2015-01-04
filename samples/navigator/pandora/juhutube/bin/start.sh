#!/bin/bash
DEFAULTPREFIX="$HOME"
export PATH="$DEFAULTPREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$DEFAULTPREFIX/lib:$LD_LIBRARY_PATH"
export SSL_CERT_PATH="$DEFAULTPREFIX/ssl/certs"
export SSL_CERT_FILE="$DEFAULTPREFIX/ssl/certs/ca-certificates.crt"

Terminal -e "youtubeplayer.sh" --title="Juhutube Console"
