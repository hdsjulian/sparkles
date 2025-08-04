include <roundedcube.scad>;
$fa = 1; $fs = $preview ? 2 : 0.5;

$battery_metal_width = 20;
$battery_wall_width = 1;
$battery_outer_width = $battery_metal_width+$battery_wall_width*2;
$battery_metal_thickness = 1.45;
$battery_metal_thickness_2 = 1.45;
$battery_length = 72.55;
$battery_height = 18.75;
$battery_outer_length = 74.7;
$battery_schnupsi_width = 4.5;
$battery_cutout_start = 6;
$battery_cutout_width = $battery_outer_width/2;
$battery_tongue_width = 7;
$battery_casing_height = $battery_height+$battery_wall_width;
module battery_base() { 
    cube([$battery_outer_width, $battery_outer_length, $battery_wall_width]);
}

module battery_wall() {
    //front
    translate([0,0,$battery_wall_width]) roundedcube([$battery_outer_width, $battery_wall_width, $battery_height], false, 0.5, "zmax");
    //back 
    translate([0,$battery_outer_length-$battery_wall_width,$battery_wall_width]) roundedcube([$battery_outer_width, $battery_wall_width, $battery_height], false, 0.5, "zmax");
    //left 
    translate([0,0, $battery_wall_width]) roundedcube([$battery_wall_width, $battery_outer_length, $battery_height], false, 0.5, "zmax");
    //right
    translate([$battery_outer_width-$battery_wall_width,0, $battery_wall_width]) roundedcube([$battery_wall_width, $battery_outer_length, $battery_height], false, 0.5, "zmax");
    //schnupsi
    translate([0, $battery_wall_width+$battery_metal_thickness, $battery_wall_width]) color("blue", 1.0) roundedcube([$battery_wall_width+$battery_schnupsi_width,$battery_wall_width/2, $battery_height], false, 0.5, "zmax");
    //schnupsi
    //translate([$outer_width, $wall_width+$metal_thickness, $wall_width]) color("green", 1.0)  roundedcube([-$wall_width-$schnupsi_width,$wall_width/2, $battery_height], false, 0.5, "zmax");    
    //schnupsi
  translate([$battery_outer_width-$battery_wall_width-$battery_schnupsi_width, $battery_wall_width+$battery_metal_thickness, $battery_wall_width]) color("purple", 1.0) roundedcube([$battery_wall_width+$battery_schnupsi_width,$battery_wall_width/2, $battery_height], false, 0.5, "zmax"); 
    //schnupsi
    //translate([$outer_width, $outer_length-$wall_width-$metal_thickness-$wall_width/2, $wall_width]) color("blue", 1.0)   roundedcube([-$wall_width-$schnupsi_width,$wall_width/2, $battery_height], false, 0.5, "zmax");    
  translate([$battery_outer_width-$battery_wall_width-$battery_schnupsi_width, $battery_outer_length-$battery_wall_width-$battery_metal_thickness_2-$battery_wall_width/2, $battery_wall_width]) color("red", 1.0) roundedcube([$battery_wall_width+$battery_schnupsi_width,$battery_wall_width/2, $battery_height], false, 0.5, "zmax"); 
      translate([0, $battery_outer_length-$battery_wall_width-$battery_metal_thickness_2-$battery_wall_width/2, $battery_wall_width]) color("red", 1.0) roundedcube([$battery_wall_width+$battery_schnupsi_width,$battery_wall_width/2, $battery_height], false, 0.5, "zmax"); 


}

module battery_cutout () {
    translate([6, 6, 0]) cube([$battery_cutout_width, $battery_outer_length-$battery_cutout_start*2, $battery_wall_width]);
     translate([$battery_outer_width/2, $battery_wall_width, 0]) cube([$battery_tongue_width, 1,$battery_wall_width*2], true);
    translate([$battery_outer_width/2, $battery_outer_length-$battery_wall_width, 0]) cube([$battery_tongue_width, 1,$battery_wall_width*2], true);   
    
}
module battery_wall_cutout() {
    translate([0, $battery_outer_length/2, $battery_outer_length+$battery_casing_height/2+$battery_wall_width]) rotate([0,90,0]) cylinder($battery_wall_width, $battery_outer_length, $battery_outer_length);
    translate([$battery_outer_width-$battery_wall_width, $battery_outer_length/2, $battery_outer_length+$battery_casing_height/2+$battery_wall_width]) rotate([0,90,0]) cylinder($battery_wall_width, $battery_outer_length, $battery_outer_length);
}
//battery_wall();
//color("red", 1.0) battery_wall_cutout() ;
module battery_casing() {
    rotate([90, 0, 90]) {
        difference() {
            battery_base();
            battery_cutout();
        }   
        difference () {
            battery_wall();
            battery_wall_cutout();
        }
    }
}
//battery_casing();
/*
module verbindungsschnupsi() {
    translate([ 0,0.5, 0]) rotate([90,0,0]) cube([3, 2.5, 0.5]); 
    translate([ -0.75,1, 0]) rotate([90,0,0]) cube([4.5, 2.5, 0.5]);   
}
*/
/*
difference() {
    translate([-$battery_casing_height, 0,0]) battery_casing();
    translate([-$battery_outer_width/2, 0, 0]) verbindungsschnupsi() ;
    //translate([-$battery_outer_width, 0, 10]) cube(100, 100, 100);
}
*/
//translate([$battery_outer_width/2,0 , 0]) schnupsi() ;