include </Users/julian/Code/sparkles/battery_casing.scad>;

$fa = 1; $fs = $preview ? 2 : 0.5;
//$fn=200;

$move = 35;


$carrier_height = 16;
$carrier_width = 8;
$pcb_thickness = 1.77;
$carrier_length = 43;
$bottom_height=10;
$pcb_width=41.3;
$pcb_height = 65;
$middle_height=$pcb_height+3;
$bottom_cylinder_outer = $carrier_length+20;
$notch_height = 20;
$notch_width = 10;
$top_height = 20;
$notch_start = $bottom_height+$middle_height+$top_height-5;
$enforcement_start = $bottom_height+$middle_height-5;
$through_hole = 2.5;
$casing_width = 1;
$cylinder_bottom_to_top = 8;
$large_cutout_width = $carrier_width+12;
$large_cutout_length = $carrier_length+4;
$upper_brim_width = 2;
$top_thickness = 2;
$reinforcement_height = 5;
$bottom_cylinder_inner = $carrier_length-10;
$battery_width = 20.5;
$battery_height = 16;
$battery_cutoff = 1;

module cylbottom() {
     cylinder(h=$bottom_height, d1=$bottom_cylinder_outer, d2=$bottom_cylinder_outer);
}
module cyltop() {
    translate([0, 0, $bottom_height+$middle_height]) difference() {
        cylinder(h=$top_height, d1=$bottom_cylinder_outer-$cylinder_bottom_to_top+$upper_brim_width, d2=0);
        cylinder(h=$top_height-$top_thickness, d1=$bottom_cylinder_outer-$cylinder_bottom_to_top-$casing_width, d2=0);
    }
}

module notch() {
    difference() {
        difference() {
            translate([0, 0, $notch_start])
                cylinder($notch_height, d1=$notch_width, d2=$notch_width);
           translate([0, 0, $bottom_height+$middle_height]) cylinder(h=$top_height, d1=$bottom_cylinder_outer-$cylinder_bottom_to_top, d2=0);
        }
     translate([0, 0, $notch_start+$notch_height-5]) rotate([90,0,0])  cylinder($notch_width, d1=$through_hole, d2=$through_hole, center=true);
     }
 }
       


module cylmiddle() {
    translate([0, 0, $bottom_height]) 
        difference() {
        cylinder($middle_height, d1=$bottom_cylinder_outer, d2=$bottom_cylinder_outer-$cylinder_bottom_to_top);
         cylinder($middle_height, d1=$bottom_cylinder_outer-$casing_width, d2=$bottom_cylinder_outer-$cylinder_bottom_to_top-$casing_width);
        }
}

module reinforcement() {
    translate([0,0,$bottom_height]) 
        difference () {
            cylinder($middle_height, d1=$bottom_cylinder_outer-$cylinder_bottom_to_top-20, d2=$bottom_cylinder_outer-$cylinder_bottom_to_top+$upper_brim_width);
            cylinder($middle_height-$reinforcement_height, d1=$bottom_cylinder_outer, d2=$bottom_cylinder_outer);
         cylinder($middle_height, d1=$bottom_cylinder_outer-$casing_width, d2=$bottom_cylinder_outer-$cylinder_bottom_to_top-$casing_width);
        }
    }
//translate([0,0,5]) cyltop();

module cutout() {
    difference() {
        translate([0,0,1]) cylinder($bottom_height, d1=$large_cutout_length, d2=$large_cutout_length);
         translate([0,0,$bottom_height/2+2]) cube([$carrier_width+2,$carrier_length+2,$bottom_height], true);
        rotate([0, 0,90])cube([$large_cutout_width+2, $large_cutout_length, $bottom_height*2], true);
    }
}
        

module cylinder_bottom() {    
    difference() {
        cylbottom();
         translate([0,0,$bottom_height/2+1]) cube([$carrier_width+1,$carrier_length+2,$bottom_height], true);
        rotate([0, 0,90])cube([$large_cutout_width, $large_cutout_length, $bottom_height*2], true);
         //cutout();
         translate([0,0,0]) cylinder(h=$bottom_height, d1=$bottom_cylinder_inner, d2=$bottom_cylinder_inner);
        
    }
}
module carrier_negative() {
        translate([$move,0,4+$carrier_height/2]) color([0,0,1]) cube([$carrier_width,37,4+$carrier_height/2], true);
    //große Aussparung
        translate([$move+$carrier_width/2,0.5,8+$carrier_height/2]) color([0,1,0]) cube([$carrier_width,$pcb_width-4,6+$carrier_height/2], true);
    //furche
    translate([$move,-0.5,2.5+$carrier_height/2]) color([1,0,0]) cube([$pcb_thickness,$pcb_width,6+$carrier_height/2], true);
    //wlan-furche
    translate([$move,-2,0+$carrier_height/2]) color(0,1,0) cube([4, 20,+$carrier_height/2], true);
    translate([$move+$carrier_width/2+$pcb_thickness/2,-16,$carrier_height-5]) color(0,1,0) cube([$carrier_width, 8, 15], true);
    }

/*module battery() {
   difference() {
            translate([0,0,$battery_height/2]) cube([$battery_width, $battery_width, $battery_height], true);
            translate([0,0,2]) cylinder($battery_height, d=19);
            translate([0,-$battery_width/2,$battery_height/2]) cube([$battery_width, $battery_cutoff, $battery_height], center=true);
            translate([0,$battery_width/2,$battery_height/2]) cube([$battery_width, $battery_cutoff, $battery_height], center=true);

        }

} */   

module carrier() { 
        difference() {
            translate([$move,0,$carrier_height/2]) color([0,0,1]) cube([$carrier_width,$carrier_length,$carrier_height], true);
            carrier_negative();
        }
    translate([$move-$battery_casing_height/2,$carrier_length/2, 0])  battery_casing();
    translate([$move-$battery_casing_height/2,-$carrier_length/2-$battery_outer_width, 0]) battery_casing();
}



module casing() {
 cylinder_bottom();
cylmiddle();
cyltop();
notch();
reinforcement();
}



//carrier_negative();
//translate([50, 0, 0]) carrier();
//casing();

carrier();
//battery();