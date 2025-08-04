$fa = 1; $fs = $preview ? 0.5 : 0.5;
$fn = $preview? 200 : 200;

$ellipse_length = 106;
$ellipse_width = 56;
$base_height = 8.5;
$casing_straight_height = 80;
$casing_roof_height = 20;
$screw_thickness = 2;
$board_width = 45.3;
$board_thickness = 1.75;
$rail_height = 25;
$rail_width = 4.5;
$rail_length = 8;
$ellipse_inner_width = $ellipse_width-2.5;
$cutout_square_width = $board_width-1.5;

$angular_cutaway_start = $base_height-2+7;
$angular_cutaway_end = $rail_height;
module elrdcyl(
   w, // width of cylinder
   d, // depth of cylinder
   h1,// straight height of cylinder
   h2 // height of rounded top
   ) {
   intersection(){
     union(){
       scale([w/2,d/2,1])cylinder(r=1,h=h1);  // cylinder
       translate([0,0,h1])scale([w/2,d/2,h2])sphere(r=1);  // top
     }
     scale([w/2,d/2,1])cylinder(r=1,h=h1+h2); // only needed if h2>h1 
   }
}
module cover_main() {
elrdcyl($ellipse_length,$ellipse_width,$casing_straight_height,$casing_roof_height);
}

$notch_height = 20;
$notch_width = 12;
$notch_start = $casing_straight_height+$casing_roof_height-3;
$through_hole = 2.8;
//$notch_start = 0;
$pitch = 5;
$screw_height = 13;
$screw_width = 10;
$screwtop_height = 8;
$screwtop_radius = 6;



$cutout_insert_width = 3;
                
$battery_to_rail_distance = 0;
$battery_cutout_cylinder_width = 2;
$cutout_insert_length = ($ellipse_inner_width/2)-$screw_thickness;


    difference() {
         translate([-($board_width+$rail_width)/2, 100-$rail_length/2, $base_height]) cube([$rail_width, $rail_length, $rail_height]);
         translate([-$board_width/2, 100-$board_thickness/2, $base_height-2]) cube([$board_width, $board_thickness, $rail_height+15]);
         translate([$rail_width, 100, $rail_height+24.5]) cube([$board_width+$rail_width, $rail_length, $rail_height+25], true);
     }
    difference() {
         translate([($board_width)/2-1.5, 100-$rail_length/2, $base_height]) cube([$rail_width, $rail_length, $rail_height]);
         translate([-$board_width/2, 100-$board_thickness/2, $base_height-2]) cube([$board_width, $board_thickness, $rail_height+15]);
         translate([$rail_width, 100, $rail_height+24.5]) cube([$board_width+$rail_width, $rail_length, $rail_height+25], true);
     }

