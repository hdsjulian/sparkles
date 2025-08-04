$battery_radius = 9;
$board_thickness = 1.75;
$board_width = 45;
$board_height = 65;
$led_placement = 10;
//color("blue", 1.0) translate([0, $board_thickness/2, $board_height-$led_placement])rotate([90, 0, 0])  cylinder(22.5, 3, 45);
translate ([$battery_radius/2+$board_width/2, $battery_radius, 0]) cylinder(65, $battery_radius,$battery_radius);
translate ([$battery_radius/2+$board_width/2, -$battery_radius, 0]) cylinder(65, $battery_radius,$battery_radius);
translate([0, 0, $board_height/2]) rotate ([0, 0,90]) cube([$board_thickness, $board_width, $board_height], true);
translate ([-$battery_radius/2-$board_width/2, $battery_radius, 0]) cylinder(65, $battery_radius,$battery_radius);
translate ([-$battery_radius/2-$board_width/2, -$battery_radius, 0]) cylinder(65, $battery_radius,$battery_radius);

