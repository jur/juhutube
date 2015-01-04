#!/bin/bash
PND="${1:-juhutube}"
WORKDIR="$PWD"
cd "`dirname \"$0\"`" || exit -1
SCRIPTDIR="$PWD"
cd "$WORKDIR"
xmllint --noout --schema "$SCRIPTDIR/PXML_schema.xsd" "$PND/PXML.xml"
if [ -e "$PND.tmp" ]; then
	rm "$PND.tmp"
fi
if [ -e "$PND.tmp" ]; then
	rm "$PND.pnd"
fi
mksquashfs "$PND/" "$PND.tmp" || exit -1
cat "$PND.tmp" "$PND/PXML.xml" "$PND/icon.png" > "$PND.pnd" || exit -1
rm "$PND.tmp"
