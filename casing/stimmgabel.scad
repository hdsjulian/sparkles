$fa = 1; $fs = $preview ? 0.5 : 0.5;
$fn = $preview? 200 : 200;
$base_height = 5;
$base_width = 12.5;
$tube_height = 20;
$tube_width = 4;
$tube_wall = 0.5;



cylinder($base_height, $base_width, $base_width);
translate([0,0,$base_height]) difference() {
    cylinder($tube_height, $tube_width+$tube_wall*2,  $tube_width+$tube_wall*2);
    cylinder($tube_height, $tube_width, $tube_width);
    };