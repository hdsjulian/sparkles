include <threads.scad>;

$fa = 1; $fs = $preview ? 2 : 0.5;
$fn = $preview? 20 : 200;
$notch_height = 20;
$notch_width = 12;
//$notch_start = $casing_straight_height+$casing_roof_height-3;
$through_hole = 2.5;
$notch_start = 0;
$pitch = 5;
$screw_height = 13;
$screw_width = 10;
$screwtop_height = 5;
$screwtop_radius = 6;

module notch() {
    difference() {
        difference() {
            translate([0, 0, $notch_start])
                cylinder($notch_height, d1=$notch_width+$notch_width/2, d2=$notch_width);
           //cover_main();
        }
//     translate ([0,0,$notch_height-$screw_height]) ScrewThread($screw_width, $screw_height, $pitch, 75);
     
//     module ScrewHole(outer_diam, height, position=[0,0,0], rotation=[0,0,0], pitch=0, tooth_angle=30, tolerance=0.4, tooth_height=0)

     }


 }
      ScrewHole($screw_width, $screw_height, position=[0,0,$notch_height-$screw_height], rotation=[0,0,0], pitch=$pitch, tooth_angle=75, tolerance=0.4, tooth_height=0) notch();

 
 

//translate([0, 20, 0]) screw();
// module ScrewThread(outer_diam, height, pitch=0, tooth_angle=30, tolerance=0.4, tip_height=0, tooth_height=0, tip_min_fract=0)
