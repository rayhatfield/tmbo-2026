<?php
// Test images for the tall image scroll hint: a long comic, one just taller
// than a phone screen, a landscape and a tiny one. Rerunnable. Run it with
// scripts/seed-scroll-hint-images.sh, not by hand.
$root = getenv("TMBO_ROOT") ?: "/var/www/html";
$db = new mysqli(getenv("DB_HOST") ?: "db", "tmbo", "shortbus", "tmbo");
$images = array(
	array(990001, "long_comic.png", 1170, 6000, array(109, 89, 122)),
	array(990002, "just_taller_than_a_phone.png", 900, 1500, array(224, 122, 95)),
	array(990003, "landscape.png", 1600, 1200, array(42, 157, 143)),
	array(990004, "tiny.png", 200, 150, array(233, 196, 106)),
);
foreach($images as $i => $image) {
	list($id, $name, $w, $h, $rgb) = $image;
	$day = 10 + $i;
	$dir = "$root/offensive/uploads/2026/11/$day/image";
	@mkdir("$dir/thumbs", 0777, true);
	$im = imagecreatetruecolor($w, $h);
	for($y = 0; $y < $h; $y++) {
		$t = $y / $h;
		$c = imagecolorallocate($im, $rgb[0] + (255 - $rgb[0]) * $t, $rgb[1] + (255 - $rgb[1]) * $t, $rgb[2] + (255 - $rgb[2]) * $t);
		imageline($im, 0, $y, $w, $y, $c);
	}
	$black = imagecolorallocate($im, 0, 0, 0);
	imagestring($im, 5, 10, 10, "top of $name", $black);
	imagestring($im, 5, 10, $h - 30, "bottom of $name", $black);
	imagepng($im, "$dir/{$id}_$name");
	$thumb = imagescale($im, 100);
	imagejpeg($thumb, "$dir/thumbs/th$id.jpg");
	$db->query("REPLACE INTO offensive_uploads (id, userid, filename, type, status, timestamp)
		VALUES ($id, 1, '$name', 'image', 'normal', '2026-11-$day 12:00:00')");
	echo "/offensive/pages/pic.php?id=$id\n";
}
