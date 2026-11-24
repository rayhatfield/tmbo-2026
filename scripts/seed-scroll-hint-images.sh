#!/bin/sh
# Seeds the scroll hint test images into the running Docker dev stack and
# prints URLs for this machine and for a phone on the same network.
set -e
cd "$(dirname "$0")/.."
paths=$(docker compose exec -T php php < scripts/seed-scroll-hint-images.php)
ip=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || hostname -I 2>/dev/null | cut -d" " -f1)
echo "on this machine:"
for p in $paths; do echo "  https://localhost$p"; done
if [ -n "$ip" ]; then
	echo "on your phone (same wifi; accept the certificate warning, log in as admin / [nsfw]):"
	for p in $paths; do echo "  https://$ip$p"; done
fi
