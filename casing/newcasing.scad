include <battery_casing.scad>;
include <cyl_head_bolt.scad>;
include <threads.scad>;

$fa = 1; $fs = $preview ? 0.5 : 0.5;
$fn = $preview? 200 : 200;

$ellipse_length = 106;
$ellipse_width = 56;
$base_height = 10;
$casing_straight_height = 85;
$casing_roof_height = 20;
$screw_thickness = 2;
$board_width = 45.3;
$board_thickness = 1.75;
$rail_height = 15;
$rail_width = 4.5;
$rail_length = 8;
$ellipse_inner_width = $ellipse_width-3;
$ellipse_inner_length = $ellipse_length-3;
$cutout_square_width = $board_width-1.5;

$angular_cutaway_start = $base_height-2+7;
$angular_cutaway_end = $base_height+$rail_height;

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
elrdcyl($ellipse_length+0.3,$ellipse_width+0.3,$casing_straight_height,$casing_roof_height);
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



module notch() {
  difference () {
        difference() {
            translate([0, 0, $notch_start])
                cylinder($notch_height, d1=$notch_width+$notch_width/2, d2=$notch_width);
       cover_main();
        }
        translate ([0,0,$notch_start+$notch_height-$screw_height]) ScrewThread($screw_width, $screw_height, $pitch);
   }
 }
 
 


module cover() {

    translate([0, 0, 0]) difference() {
        cover_main();
        translate([0,0,-1]) elrdcyl($ellipse_length-1.7,$ellipse_width-1.7,$casing_straight_height, $casing_roof_height-1);
        translate([-$ellipse_length/2, 0, $base_height/2+2]) rotate([0,90, 0]) cylinder($ellipse_length, $screw_thickness, $screw_thickness);
    }
    ScrewHole($screw_width, $screw_height, position=[0,0,$notch_start+$notch_height-$screw_height], rotation=[0,0,0], pitch=$pitch, tooth_angle=75, tolerance=0.4, tooth_height=0) notch();
}

module bottom_base() {
    difference() {
        //base
        translate([0, 100, 0]) elrdcyl($ellipse_inner_length,$ellipse_inner_width,$base_height, 0);
        //screw
        translate([-$ellipse_length/2, 100, $base_height/2-0.5]) rotate([0,90, 0]) cylinder($ellipse_length, $screw_thickness, $screw_thickness);
        
        
    }
    //rail
    //cutting off
           
        //screw
    //long rail
    
    difference() {
         translate([-($board_width+$rail_width)/2, 100-$rail_length/2, $base_height]) cube([$rail_width, $rail_length, $rail_height+27]);
         translate([-$board_width/2, 100-$board_thickness/2, $base_height-2]) cube([$board_width, $board_thickness, $base_height-2+44]);
     }
    difference() {
         mirror([1, 0, 0]) translate([-($board_width+$rail_width)/2, 100-$rail_length/2, $base_height]) cube([$rail_width, $rail_length, $rail_height]);
         translate([-$board_width/2, 100-$board_thickness/2, $base_height-2]) cube([$board_width, $board_thickness, $rail_height]);
     }
}
//xxx
    difference() {

        }
        



module bottom() {

    difference() {
        bottom_base();
        translate([0, 100, $base_height/2]) cube([$cutout_square_width,$board_width+2, $casing_straight_height], true);
        
        //cutout because of Resistors
        translate ([-$board_width/2+0.5, 100-$rail_length/2, $base_height-2]) cube([2, $rail_length/2, 65]);

    }
    difference() {
         translate([0, 100, 0]) elrdcyl($ellipse_length-1.5,$ellipse_inner_width,$base_height, 0);
        translate([0, 100, 0]) elrdcyl($ellipse_length-10,$ellipse_inner_width-10,$base_height+$rail_height, 0);
    }
    
    //here
    //screw hole 1 
    difference() {
        translate([$board_width/2-5,100, $base_height]) cube([5, $rail_length/2, $rail_height], false);
            translate([$board_width/2-2.5,100, $base_height-2+12.5]) rotate([90, 0, 0]) cylinder(10, d1=3, d2=3, center=true);
            
            translate([$board_width/2-5,100, 0]) cube([5, $rail_length/2, $base_height-2+7], false);
color("blue", 1.0) translate([$cutout_square_width/2-$rail_width,100+$rail_length/2, $angular_cutaway_start]) rotate([90, 0, 0]) cylinder(5,5,5,$fn=3);
translate([$cutout_square_width/2-$rail_width,100+$rail_length/2, $angular_cutaway_end]) rotate([90, 0, 0]) cylinder(5,5,5,$fn=3);
         }
    //screw hole 2
    difference() {
        translate([-$board_width/2,100+$board_thickness/2, $base_height-2+32]) cube([5, $rail_length/2-$board_thickness/2, 12], false);
    // dirty cutout trick. fix this.
    translate([-$cutout_square_width/2+$rail_width,100+$rail_length/2, $base_height-2+34
    ]) rotate([90, 180, 0]) cylinder(5,5,5,$fn=3);
    translate([-$cutout_square_width/2+$rail_width,100+$rail_length/2, $base_height-2+34
    ]) rotate([90, 180, 0]) cube([5,5,5]);
    
    }
}


module entire_bottom() {
    difference() {
            bottom();
        //one side cutout
        translate([-($board_width)/2-$rail_width/2-$battery_outer_width, 100-$battery_outer_width/2, $base_height-1.1]) cube([$battery_outer_width, $ellipse_inner_width, 1.1]);
          //left
          translate([-$board_width/2-$rail_width/2-$battery_to_rail_distance-$battery_outer_width, 100+($ellipse_inner_width)/2,  $base_height-1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2, $battery_cutout_cylinder_width/2);
          //right
    translate([-$board_width/2-$rail_width/2-$battery_to_rail_distance, 100+($ellipse_inner_width)/2,  $base_height-1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2, $battery_cutout_cylinder_width/2);
        
        //other side cutout
        
          translate([($board_width)/2+$rail_width/2, 100-$battery_outer_width/2, $base_height-1.1]) cube([$battery_outer_width, $ellipse_inner_width, 1.1]);
          //left
          translate([$board_width/2+$rail_width/2+$battery_to_rail_distance+$battery_outer_width, 100+($ellipse_inner_width)/2,  $base_height-1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2, $battery_cutout_cylinder_width/2);
      
          //right
    translate([$board_width/2+$rail_width/2+$battery_to_rail_distance, 100+($ellipse_inner_width)/2,  $base_height-1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2, $battery_cutout_cylinder_width/2);
      
      //board
      translate([-$board_width/2, 100-$board_thickness/2, $base_height-2]) cube([$board_width, $board_thickness, $rail_height+2]);
      //screws
          translate([-$ellipse_length/2, 100, $base_height/2-0.5]) rotate([180,90, 0])  hole_through(name="M3", l=12, cld=0.1, h=0, hcld=0.4);
    translate([$ellipse_length/2, 100, $base_height/2-0.5]) rotate([0,90, 0])  hole_through(name="M3", l=12, cld=0.1, h=0, hcld=0.4);
    //screwhole left up
        translate([-$board_width/2+2.5,100, $base_height-2+40.666]) rotate([90, 0, 0]) cylinder(10, d1=3, d2=3, center=true);
        
    }
}

        
        
module screw() {   
    difference() {
        union() {
            translate ([0, 0, $screwtop_height])ScrewThread($screw_width, $screw_height, $pitch, 75);
            cylinder($screwtop_height, d1=$notch_width, d2=$notch_width);
        }
                   translate([0, 0, $screwtop_height/2]) rotate([90, 0, 0]) cylinder($notch_width, d1=$through_hole, d2=$through_hole, center=true);

   }
}

//translate([0, 100, 0]) screw();
//cover();
difference() {
//yyy
    entire_bottom();
//   bottom();
   }
//screw();

module fittings() {
translate([-$battery_outer_width-0.1, $battery_casing_height/2+$cutout_insert_length,  -0.1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2-0.3, $battery_cutout_cylinder_width/2-0.3);
translate([0.1, $battery_casing_height/2+$cutout_insert_length,  -0.1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2-0.3, $battery_cutout_cylinder_width/2-0.3);
}
module battery_full() {
    difference() {
        fittings();
        translate([-50, $battery_casing_height-2.5, -10]) cube([100, 100, 100]);
    }
 translate([0, $battery_casing_height/2, 0]) rotate([0, 0, 90]) battery_casing();

}
//battery_full();
//    translate([0, 100+$battery_outer_width/2, 0]) cube([100, 100, 100]);
//translate([-$cutout_square_width/2-$rail_width-$battery_to_rail_distance-$battery_outer_width-$battery_cutout_cylinder_width/2, 100+($ellipse_inner_width)/2,  $base_height-1]) rotate([90, 270, 0]) cylinder($cutout_insert_length, $battery_cutout_cylinder_width/2, $battery_cutout_cylinder_width/2);


// translate([45/2+$battery_outer_width, 100-$battery_outer_width/2, 10]) rotate([0, 0, 90]) battery_casing();
 
 

/*
color("blue", 1.0) translate([-($board_width)/2-$rail_width/2, 100-$battery_outer_width/2+$ellipse_inner_width, $base_height-1]) rotate([0,0,180]) cube([10, $ellipse_inner_width, 1]);
*/