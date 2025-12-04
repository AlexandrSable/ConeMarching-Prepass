// ──────────────────────────────────────────────────────────────────────── //
//                             SDF SCENE COMPUTE                            //
// ──────────────────────────────────────────────────────────────────────── //
float M_PI = 3.14159265358979323846;

vec4 qSquare( in vec4 q ){
    return vec4(q.x*q.x - q.y*q.y - q.z*q.z - q.w*q.w, 2.0*q.x*q.yzw);
}
vec4 qCube( in vec4 q ){
    vec4  q2 = q*q;
    return vec4(q.x  *(    q2.x - 3.0*q2.y - 3.0*q2.z - 3.0*q2.w), 
                q.yzw*(3.0*q2.x -     q2.y -     q2.z -     q2.w));
}
float qLength2( in vec4 q ) { return dot(q,q); }

// Adaptive iteration count based on march distance (close-up optimization)
int getAdaptiveIterations(float marchDistance) {
    // Far away: full iterations, close-up: reduced iterations
    if (marchDistance < 5.0) return 80;   // Very close: minimal iterations
    if (marchDistance < 15.0) return 120; // Medium: reduced
    return 200;                           // Far: full precision
}

float Gacket( in vec3 p0 ){
    vec4 p = vec4(p0, 1.);
    for(int i = 0; i < 8; i++){
        p.xyz = mod(p.xyz-1.,2.)-1.;
        p*=1.4/dot(p.xyz,p.xyz);
    }
    return (length(p.xz/p.w)*0.25);
}

vec2  JuliaSet( in vec3 p ){
    vec4 z = vec4( p, 0.0 );
    float dz2 = 1.0;
	float m2  = 0.0;
    float n   = 0.0;
    const vec4  kC = vec4(-2,6,15,-6)/22.0;
    //const vec4  kC = vec4(1,-0.4,-.4,.2);
    #ifdef TRAPS
    float o   = 1e10;
    #endif
    
    for( int i=0; i<200; i++ ) 
	{
        // z' = 3z² -> |z'|² = 9|z²|²
		dz2 *= 9.0*qLength2(qSquare(z));
        
        // z = z³ + c		
		z = qCube( z ) + kC;
        
        // stop under divergence		
        m2 = qLength2(z);		

        // orbit trapping : https://iquilezles.org/articles/orbittraps3d
        #ifdef TRAPS
        o = min( o, length(z.xz-vec2(0.45,0.55))-0.1 );
        #endif
        
        // exit condition
        if( m2>256.0 ) break;				 
		n += 1.0;
	}
   
	// sdf(z) = log|z|·|z|/|dz| : https://iquilezles.org/articles/distancefractals
	float d = 0.25*log(m2)*sqrt(m2/dz2);
    
    #ifdef TRAPS
    d = min(o,d);
    #endif
    #ifdef CUT
    d = max(d, p.y);
    #endif
    
	return vec2(d, n);        
}
	
vec2  Rot2D(in vec2 q, float a){
    vec2 cs;
    cs = sin (a + vec2 (0.5 * M_PI, 0.));
    return vec2 (dot (q, vec2 (cs.x, - cs.y)), dot (q.yx, cs));
}

float PrBoxDf(in vec3 p, in vec3 b){
    vec3 d;
    d = abs (p) - b;
    return min (max (d.x, max (d.y, d.z)), 0.) + length (max (d, 0.));
}

float FunnyTorus(in vec3 p){
    vec3 b;
    float r, a;
    const float nIt = 5., sclFac = 2.4;
    b = (sclFac - 1.) * vec3 (1., 1.125, 0.625);
    r = length (p.xz);
    a = (r > 0.) ? atan (p.z, - p.x) / (2. * M_PI) : 0.;
    p.x = mod (16. * a + 1., 2.) - 1.;
    p.z = r - 32. / (2. * M_PI);
    p.yz = Rot2D (p.yz, M_PI * a);
    for (float n = 0.; n < nIt; n ++) {
      p = abs (p);
      p.xy = (p.x > p.y) ? p.xy : p.yx;
      p.xz = (p.x > p.z) ? p.xz : p.zx;
      p.yz = (p.y > p.z) ? p.yz : p.zy;
      p = sclFac * p - b;
      p.z += b.z * step (p.z, -0.5 * b.z);
    }
    return 0.8 * PrBoxDf (p, vec3 (1.)) / pow (sclFac, nIt);
}

float SerptTri(vec3 p){
    float i,d=1.,b=1.73;
    vec3 Q=mod(p,b*2.)-b;
    for(int j=0;j++<6;){
      Q=abs(Q);
      if(Q.y>Q.x)Q.xy=Q.yx;
      if(Q.z>Q.x)Q.zx=Q.xz;
      Q*=2.;
      Q.x-=b;
    }
    return (dot(abs(Q),vec3(1)/b)-1.)/64.;
}
	
float IDK(vec3 p){
    float s=3., offset=8., e;
    for(int i=0;i++<9;p=vec3(2,4,2)-abs(abs(p)*e-vec3(4,4,2)))
      s*=e=max(1.,(8.+offset)/dot(p,p));
    return min(length(p.xz),p.y)/s;
}

	
#define D (dot(sin(Q),cos(Q.yzx))+1.3)
float tunnels(vec3 p){
  	vec3 Q;
  	float i,d=1.;
  	Q=p, d=D, Q.x+=M_PI, d=min(d,D);
  	Q.y+=M_PI;
    d=min(d,D);
  	Q*=30.;
  	d=max(abs(d),(abs(D-1.3)-.5)/30.);
  	return d*.6;
}

vec2 mapSDF(vec3 p) {
    float objectSDF = 1e6;
    float objIndexSDF = 0.0;
    
    // Render planets from SSBO
    for (int i = 0; i < u_numPlanets; i++) {
        vec3 planetPos = planets[i].positionRadius.xyz;
        float planetRadius = planets[i].positionRadius.w;
        vec3 planetColor = planets[i].colorGravity.xyz;
        
        // Distance to planet surface (signed distance function)
        float distToPlanet = distance(p, planetPos) - planetRadius;
        
        if (distToPlanet < objectSDF) {
            objectSDF = distToPlanet;
            // Encode planet index for material/color assignment
            objIndexSDF = float(i) + 1.0;
        }
    }
    
    // // Also check for fractal geometry (tunnel structure)
    // // Only if planets are far away (optimization)
    // if (objectSDF > 1.0) {
    //     float fractalSDF = tunnels(p);
    //     if (fractalSDF < objectSDF) {
    //         objectSDF = fractalSDF;
    //         objIndexSDF = 0.0;  // Fractal index
    //     }
    // }

    return vec2(objectSDF, objIndexSDF);
}
