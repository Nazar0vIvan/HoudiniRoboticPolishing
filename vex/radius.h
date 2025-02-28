#include "$HIP/vex/functions.h"

// uv - unit vector
// for poly
function vector get_uvy(vector pt1; vector pt2; vector pt3; vector pt0; int is_cx) {
    vector2 uvy = polynorm(poly(pt1, pt2, pt3), x0); // normal сoeffs at pt0 
		uvy = ((uvy[1] < 0 && is_cx) || (uvy[1] > 0 && !is_cx )) ? uvy *= -1 : uvy;
		addprim(0, "polyline", addpoint(0, pt1), addpoint(0, pt1+uvy));
    return uvy;
}

// for circ
function vector get_uvy(vector pt1; vector pt2) {
    return normalize(pt1 - pt2);
}

function vector get_uvx(vector pt1; vector pt2; vector pt3) {
    float vplane[] = plane(pt1, pt2, pt3);
    return normalize(set(vplane[0], vplane[1], vplane[2]));
}

function matrix get_transform(vector uvx; vector uvy; vector uvz; vector pt) {
    return set(uvx[0], uvx[1], uvx[2], 0., 
               uvy[0], uvy[1], uvy[2], 0., 
               uvz[0], uvz[1], uvz[2], 0., 
               pt[0],  pt[1],  pt[2],  1.);
}

// circ data: (yc, r)  - circle center; (yt, zt) - touch point of the v line and the circle
// return radius circ center and touch point !in attached coor sys!
function vector4 get_radius_circ(vector pt1; vector pt2; float r; matrix T) {
    // circ in YZ coor sys
		matrix TI = invert(T);
    vector4 pt1T = TI*set(pt1[0], pt1[1], pt1[2], 1.0),
            pt2T = TI*set(pt2[0], pt2[1], pt2[2], 1.0);
    vector2 tangent = linsolve(pt1T[1], pt2T[1], pt1T[2], pt2T[2]);
    float k = tangent[0],
					b = tangent[1];

    // abscissa of the circ center
    float yc_max = (-b+r*(1+sqrt(k*k+1)))/k,
					yc_min = (-b+r*(1-sqrt(k*k+1)))/k;
    
    // abscissa of the touch point
    float yt_max = 1/k*(-b+r*(1+1/(sqrt(k*k+1)))),
			    yt_min = 1/k*(-b+r*(1-1/(sqrt(k*k+1))));
    
    float yc = k > 0 ? yc_max : yc_min,
					yt = k > 0 ? yt_max : yt_min,
					zt = k*yt+b;
    
    return set(yc,r,yt,zt); 
}

function vector edge_circ_center(int ptnums[]; matrix3 R; matrix3 RT) {
    int n = len(ptnums);
    vector pt0 = point(0, "P", ptnums[0]);
    vector ptm = point(0, "P", ptnums[floor(n/2)]);
    vector ptn = point(0, "P", ptnums[n-1]);
    return R*set(vector(circ(RT*pt0, RT*ptm, RT*ptn))); // circ center in 0 coor sys
}

function int[] addpoints_radius_sector(vector4 circ; int npt; matrix T; string blade_part; int sector_num) {
    float yc = circ[0],
					r = circ[1],
					yt = circ[2],
					zt = circ[3];
    vector2 clipline = linsolve(yc,yt,r,zt);
    float k = clipline[0],
					b = clipline[1];
    float begin_angle = k > 0 ? -PI+atan(k) : PI+atan(k),
			    end_angle = PI/2-atan(k);
    float step = end_angle/npt; 
    float t = 0.;
    float y1, z1;
    vector4 ptc;
    int ptnums[], ptnum;
    for(int i = 0; i < npt; i++) {
        y1 = yc + r*cos(t+begin_angle);
        z1 = r*(1 + sin(t+begin_angle));
        ptc = T*set(0., y1, z1, 1.);
        ptnum = addpoint(0, vector(ptc));
        setpointattrib(0, "blade_part", ptnum, "r_"+blade_part);
        setpointattrib(0, "pf_num", ptnum, sector_num);
        push(ptnums, ptnum);
        t += step;
    }
    return ptnums;
}

// for cx and cv
function int[] addpoints_radius(int i; float r; int npts; int ptnums[]; int above_pf_ptnums[]; matrix3 R; matrix3 RT; int sectors_count) {
    string blade_part = point(0, "blade_part", above_pf_ptnums[0]);
    int is_cx = blade_part == "cx" ? 1 : 0; // cx or cv: cx -> 1; cv -> 0

    int n = len(ptnums);
    int j = i == 0 ? 1 : i == n-1 ? n-2 : i;
    vector pt1 = point(0, "P", ptnums[j-1]);
    vector pt2 = point(0, "P", ptnums[j]);
    vector pt3 = point(0, "P", ptnums[j+1]);
    vector pt4 = point(0, "P", above_pf_ptnums[i]);
    
    vector pt0 = i == 0 ? pt1 : i == n-1 ? pt3 : pt2; // target point    
    vector uvy = R*get_uvy(RT*pt1, RT*pt2, RT*pt3, RT*pt0, is_cx);   
    vector uvx = get_uvx(pt0 + uvy, pt0, pt4);
    vector uvz = cross(uvx, uvy);
    
    matrix T = get_transform(uvx, uvy, uvz, pt0);
    vector4 circ = get_radius_circ(pt0, pt4, r, T);
    return addpoints_radius_sector(circ, npts, T, blade_part, -2-i-sectors_count);
}

// for edges
function int[] addpoints_radius(int i; float r; int npts; int ptnums[]; int above_pf_ptnums[]; matrix3 R; matrix3 RT; vector ptc; int sectors_count) {
    string blade_part = point(0, "blade_part", above_pf_ptnums[0]);
    
    vector pt1 = point(0, "P", ptnums[i]),
					 pt2 = point(0, "P", above_pf_ptnums[i]); 
    
    vector uvy = get_uvy(pt1, ptc),
           uvx = get_uvx(pt1 + uvy, pt1, pt2),
           uvz = cross(uvx, uvy);
    
    matrix T = get_transform(uvx, uvy, uvz, pt1);
    vector4 circ = get_radius_circ(pt1, pt2, r, T);
    return addpoints_radius_sector(circ, npts, T, blade_part, -2-i-sectors_count);
}