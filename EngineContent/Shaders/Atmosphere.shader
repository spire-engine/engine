module Atmosphere
{
    /**
    * Precomputed Atmospheric Scattering
    * Copyright (c) 2008 INRIA
    * All rights reserved.
    *
    * Redistribution and use in source and binary forms, with or without
    * modification, are permitted provided that the following conditions
    * are met:
    * 1. Redistributions of source code must retain the above copyright
    *    notice, this list of conditions and the following disclaimer.
    * 2. Redistributions in binary form must reproduce the above copyright
    *    notice, this list of conditions and the following disclaimer in the
    *    documentation and/or other materials provided with the distribution.
    * 3. Neither the name of the copyright holders nor the names of its
    *    contributors may be used to endorse or promote products derived from
    *    this software without specific prior written permission.
    *
    * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
    * THE POSSIBILITY OF SUCH DAMAGE.
    */

    /**
    * Author: Eric Bruneton
    */

    /*const*/ float SUN_INTENSITY = 100.0;

    /*const*/ vec3 earthPos = vec3(0.0, 6360010.0, 0.0);

    // ----------------------------------------------------------------------------
    // PHYSICAL MODEL PARAMETERS
    // ----------------------------------------------------------------------------

    /*const*/ float SCALE = 1000.0;

    /*const*/ float Rg = 6360.0 * SCALE;
    /*const*/ float Rt = 6420.0 * SCALE;
    /*const*/ float RL = 6421.0 * SCALE;

    /*const*/ float AVERAGE_GROUND_REFLECTANCE = 0.1;

    // Rayleigh
    /*const*/ float HR = 8.0 * SCALE;
    /*const*/ vec3 betaR = vec3(5.8e-3, 1.35e-2, 3.31e-2) / SCALE;

    // Mie
    // DEFAULT
    float HM = 1.2f * SCALE;
    vec3 betaMSca = vec3(20e-3) / SCALE;
    vec3 betaMEx = betaMSca / 0.9;
    /*const*/ float mieG = 0.8;
    /*const*/ float g = 9.81;
    /*const*/ float M_PI = 3.141592657;

    // ----------------------------------------------------------------------------
    // NUMERICAL INTEGRATION PARAMETERS
    // ----------------------------------------------------------------------------

    /*const*/ int TRANSMITTANCE_INTEGRAL_SAMPLES = 500;
    /*const*/ int INSCATTER_INTEGRAL_SAMPLES = 50;
    /*const*/ int IRRADIANCE_INTEGRAL_SAMPLES = 32;
    /*const*/ int INSCATTER_SPHERICAL_INTEGRAL_SAMPLES = 16;

    // ----------------------------------------------------------------------------
    // PARAMETERIZATION OPTIONS
    // ----------------------------------------------------------------------------

    /*const*/ int TRANSMITTANCE_W = 256;
    /*const*/ int TRANSMITTANCE_H = 64;

    /*const*/ int SKY_W = 64;
    /*const*/ int SKY_H = 16;

    /*const*/ int RES_R = 32;
    /*const*/ int RES_MU = 128;
    /*const*/ int RES_MU_S = 32;
    /*const*/ int RES_NU = 8;

    #define TRANSMITTANCE_NON_LINEAR
    #define INSCATTER_NON_LINEAR

    // ----------------------------------------------------------------------------
    // PARAMETERIZATION FUNCTIONS
    // ----------------------------------------------------------------------------
    require Texture2D transmittanceSampler;
    require Texture2D skyIrradianceSampler;
    require Texture3D inscatterSampler;
    require SamplerState linearSampler;

    vec2 getTransmittanceUV(float r, float mu)
    {
        float uR; float uMu;
    #ifdef TRANSMITTANCE_NON_LINEAR
        uR = sqrt((r - Rg) / (Rt - Rg));
        uMu = atan((mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;
    #else
        uR = (r - Rg) / (Rt - Rg);
        uMu = (mu + 0.15) / (1.0 + 0.15);
    #endif
        return vec2(uMu, uR);
    }
	
    void getTransmittanceRMu(vec2 fragCoord, out float r, out float muS)
    {
        r = fragCoord.y / float(TRANSMITTANCE_H);
        muS = fragCoord.x / float(TRANSMITTANCE_W);
    #ifdef TRANSMITTANCE_NON_LINEAR
        r = Rg + (r * r) * (Rt - Rg);
        muS = -0.15 + tan(1.5 * muS) / tan(1.5) * (1.0 + 0.15);
    #else
        r = Rg + r * (Rt - Rg);
        muS = -0.15 + muS * (1.0 + 0.15);
    #endif
    }
	
    vec2 getIrradianceUV(float r, float muS)
    {
        float uR = (r - Rg) / (Rt - Rg);
        float uMuS = (muS + 0.2) / (1.0 + 0.2);
        return vec2(uMuS, uR);
    }
	
    void getIrradianceRMuS(vec2 fragCoord, out float r, out float muS)
    {
        r = Rg + (fragCoord.y - 0.5) / (float(SKY_H) - 1.0) * (Rt - Rg);
        muS = -0.2 + (fragCoord.x - 0.5) / (float(SKY_W) - 1.0) * (1.0 + 0.2);
    }
	
    vec4 texture4D(Texture3D table, float r, float mu, float muS, float nu)
    {
        float H = sqrt(Rt * Rt - Rg * Rg);
        float rho = sqrt(r * r - Rg * Rg);
    #ifdef INSCATTER_NON_LINEAR
        float rmu = r * mu;
        float delta = rmu * rmu - r * r + Rg * Rg;
        vec4 cst = rmu < 0.0 && delta > 0.0 ? vec4(1.0, 0.0, 0.0, 0.5 - 0.5 / float(RES_MU)) : vec4(-1.0, H * H, H, 0.5 + 0.5 / float(RES_MU));
        float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
        float uMu = cst.w + (rmu * cst.x + sqrt(delta + cst.y)) / (rho + cst.z) * (0.5 - 1.0 / float(RES_MU));
        // paper formula
        //float uMuS = 0.5 / float(RES_MU_S) + max((1.0 - exp(-3.0 * muS - 0.6)) / (1.0 - exp(-3.6)), 0.0) * (1.0 - 1.0 / float(RES_MU_S));
        // better formula
        float uMuS = 0.5 / float(RES_MU_S) + (atan(max(muS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5 * (1.0 - 1.0 / float(RES_MU_S));
    #else
        float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));
        float uMu = 0.5 / float(RES_MU) + (mu + 1.0) / 2.0 * (1.0 - 1.0 / float(RES_MU));
        float uMuS = 0.5 / float(RES_MU_S) + max(muS + 0.2, 0.0) / 1.2 * (1.0 - 1.0 / float(RES_MU_S));
    #endif
        float lerp = (nu + 1.0) / 2.0 * (float(RES_NU) - 1.0);
        float uNu = floor(lerp);
        lerp = lerp - uNu;
        return table.Sample(linearSampler, vec3((uNu + uMuS) / float(RES_NU), uMu, uR)) * (1.0 - lerp) +
            table.Sample(linearSampler, vec3((uNu + uMuS + 1.0) / float(RES_NU), uMu, uR)) * lerp;
    }
	
    void getMuMuSNu(vec2 fragCoord, float r, vec4 dhdH, out float mu, out float muS, out float nu)
    {
        float x = fragCoord.x - 0.5;
        float y = fragCoord.y - 0.5;
    #ifdef INSCATTER_NON_LINEAR
        if (y < float(RES_MU) / 2.0)
        {
            float d = 1.0 - y / (float(RES_MU) / 2.0 - 1.0);
            d = min(max(dhdH.z, d * dhdH.w), dhdH.w * 0.999);
            mu = (Rg * Rg - r * r - d * d) / (2.0 * r * d);
            mu = min(mu, -sqrt(1.0 - (Rg / r) * (Rg / r)) - 0.001);
        }
        else
        {
            float d = (y - float(RES_MU) / 2.0) / (float(RES_MU) / 2.0 - 1.0);
            d = min(max(dhdH.x, d * dhdH.y), dhdH.y * 0.999);
            mu = (Rt * Rt - r * r - d * d) / (2.0 * r * d);
        }
        muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
        // paper formula
        //muS = -(0.6 + log(1.0 - muS * (1.0 -  exp(-3.6)))) / 3.0;
        // better formula
        muS = tan((2.0 * muS - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);
        nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
    #else
        mu = -1.0 + 2.0 * y / (float(RES_MU) - 1.0);
        muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
        muS = -0.2 + muS * 1.2;
        nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
    #endif
    }
	

    // ----------------------------------------------------------------------------
    // UTILITY FUNCTIONS
    // ----------------------------------------------------------------------------

    // nearest intersection of ray r,mu with ground or top atmosphere boundary
    // mu=cos(ray zenith angle at ray origin)
    float limit(float r, float mu)
    {
        float dout = -r * mu + sqrt(r * r * (mu * mu - 1.0) + RL * RL);
        float delta2 = r * r * (mu * mu - 1.0) + Rg * Rg;
        if (delta2 >= 0.0)
        {
            float din = -r * mu - sqrt(delta2);
            if (din >= 0.0)
            {
                dout = min(dout, din);
            }
        }
        return dout;
    }

    // optical depth for ray (r,mu) of length d, using analytic formula
    // (mu=cos(view zenith angle)), intersections with ground ignored
    // H=height scale of exponential density function
    float opticalDepth(float H, float r, float mu, float d)
    {
        float a = sqrt((0.5/H)*r);
        vec2 a01 = a*vec2(mu, mu + d / r);
        vec2 a01s = sign(a01);
        vec2 a01sq = a01*a01;
        float x = a01s.y > a01s.x ? exp(a01sq.x) : 0.0;
        vec2 y = a01s / (2.3193*abs(a01) + sqrt(1.52*a01sq + 4.0)) * vec2(1.0, exp(-d/H*(d/(2.0*r)+mu)));
        return sqrt((6.2831*H)*r) * exp((Rg-r)/H) * (x + dot(y, vec2(1.0, -1.0)));
    }

    // transmittance(=transparency) of atmosphere for infinite ray (r,mu)
    // (mu=cos(view zenith angle)), intersections with ground ignored
    vec3 transmittance(float r, float mu)
    {
        vec2 uv = getTransmittanceUV(r, mu);
        return transmittanceSampler.Sample(linearSampler, uv).rgb;
    }

    // transmittance(=transparency) of atmosphere for ray (r,mu) of length d
    // (mu=cos(view zenith angle)), intersections with ground ignored
    // uses analytic formula instead of transmittance texture
    vec3 analyticTransmittance(float r, float mu, float d)
    {
        return exp(- betaR * opticalDepth(HR, r, mu, d) - betaMEx * opticalDepth(HM, r, mu, d));
    }

    // transmittance(=transparency) of atmosphere for infinite ray (r,mu)
    // (mu=cos(view zenith angle)), or zero if ray intersects ground
    vec3 transmittanceWithShadow(float r, float mu)
    {
        return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? vec3(0.0) : transmittance(r, mu);
    }

    // transmittance(=transparency) of atmosphere between x and x0
    // assume segment x,x0 not intersecting ground
    // r=||x||, mu=cos(zenith angle of [x,x0) ray at x), v=unit direction vector of [x,x0) ray
    vec3 transmittance(float r, float mu, vec3 v, vec3 x0)
    {
        vec3 result;
        float r1 = length(x0);
        float mu1 = dot(x0, v) / r;
        if (mu > 0.0)
        {
            result = min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);
        }
        else
        {
            result = min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);
        }
        return result;
    }

    // transmittance(=transparency) of atmosphere between x and x0
    // assume segment x,x0 not intersecting ground
    // d = distance between x and x0, mu=cos(zenith angle of [x,x0) ray at x)
    vec3 transmittance(float r, float mu, float d)
    {
        vec3 result;
        float r1 = sqrt(r * r + d * d + 2.0 * r * mu * d);
        float mu1 = (r * mu + d) / r1;
        if (mu > 0.0)
        {
            result = min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);
        } 
        else
        {
            result = min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);
        }
        return result;
    }

    // Rayleigh phase function
    float phaseFunctionR(float mu) {
        return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
    }

    // Mie phase function
    float phaseFunctionM(float mu) {
        return 1.5 * 1.0 / (4.0 * M_PI) * (1.0 - mieG*mieG) * pow(1.0 + (mieG*mieG) - 2.0*mieG*mu, -3.0/2.0) * (1.0 + mu * mu) / (2.0 + mieG*mieG);
    }

    // approximated single Mie scattering (cf. approximate Cm in paragraph "Angular precision")
    vec3 getMie(vec4 rayMie) { // rayMie.rgb=C*, rayMie.w=Cm,r
        return rayMie.rgb * rayMie.w / max(rayMie.r, 1e-4) * (betaR.r / betaR);
    }

    // ----------------------------------------------------------------------------
    // PUBLIC FUNCTIONS
    // ----------------------------------------------------------------------------

    // scattered sunlight between two points
    // camera=observer
    // viewdir=unit vector towards observed point
    // sundir=unit vector towards the sun
    // return scattered light and extinction coefficient
    vec3 skyRadiance(vec3 camera, vec3 viewdir, vec3 sundir, out vec3 extinction)
    {
        vec3 result;
        float r = length(camera);
        float rMu = dot(camera, viewdir);
        float mu = rMu / r;
        float r0 = r;
        float mu0 = mu;

        float deltaSq = sqrt(rMu * rMu - r * r + Rt*Rt);
        float din = max(-rMu - deltaSq, 0.0);
        if (din > 0.0) {
            camera += din * viewdir;
            rMu += din;
            mu = rMu / Rt;
            r = Rt;
        }

        if (r <= Rt) {
            float nu = dot(viewdir, sundir);
            float muS = dot(camera, sundir) / r;

            vec4 inScatter = texture4D(inscatterSampler, r, rMu / r, muS, nu);
            extinction = transmittance(r, mu);

            vec3 inScatterM = getMie(inScatter);
            float phase = phaseFunctionR(nu);
            float phaseM = phaseFunctionM(nu);
            result = inScatter.rgb * phase + inScatterM * phaseM;
        } else {
            result = vec3(0.0);
            extinction = vec3(1.0);
        }

        return result * SUN_INTENSITY;
    }

    /// compute atmosperic fog on scene geometry
    /// @param x - camera location in world space
    /// @param t - distance from camera to target object
    /// @param v - view direction
    /// @param z - frag depth
    /// @param s - sun direction
    /// @out attenuation - extinction factor of target color (finalColor = targetColor*attenuation + result)
    vec3 AtmosphericScatterSceneGeometry(vec3 x, float t, vec3 v, float z, vec3 s, out vec3 attenuation)
    {
        vec3 result = vec3(0.0f);
        attenuation = vec3(1.0f);
        float r = length(x);
        float mu = dot(x, v) / r;
        float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + Rt * Rt);
        if (d > 0.0)
        { // if x in space and ray intersects atmosphere
            // move x to nearest intersection of ray with top atmosphere boundary
            x += d * v;
            t -= d;
            mu = (r * mu + d) / Rt;
            r = Rt;
        }
        if (r < Rg + 0.015f)
        {
            float Diff = (Rg + 0.015f) - r;
            x -= Diff * v;
            t -= Diff;
            r = Rg + 0.015;
            mu = dot(x, v) / r;
        }
        if (r <= Rt) // if ray intersects atmosphere
        {
            float3 x0 = x + t * v;
            float r0 = length(x0);
            // if ray intersects atmosphere
            float nu = dot(v, s);
            float muS = dot(x, s) / r;

            float MuHorizon = -sqrt(1.0 - (Rg / r) * (Rg / r));
            mu = max(mu, MuHorizon + 0.005 + 0.15);

            float MuOriginal = mu;

            float blendRatio = clamp(exp(z) - 0.5, 0.0, 1.0);
			if (blendRatio < 1.f)
			{
				v.z = max(v.z, 0.15);
				v = normalize(v);
				float3 X1 = x + t * v;
				float R1 = length(X1);
				mu = dot(X1, v) / R1; 
			}
            float phaseR = phaseFunctionR(nu);
            float phaseM = phaseFunctionM(nu);
            vec4 inscatter = max(texture4D(inscatterSampler, r, mu, muS, nu), 0.0);
            if (t > 0.0)
			{
                vec3 x0 = x + t * v;
                float r0 = max(length(x0), r);
                float rMu0 = dot(x0, v);
                float mu0 = rMu0 / r0;
                float muS0 = dot(x0, s) / r0;

                attenuation = analyticTransmittance(r, mu, t);

                if (r0 > Rg + 0.01)
                {
                    if (blendRatio < 1.f)
				    {
                        // computes S[L]-T(x,x0)S[L]|x0
                        inscatter = max(inscatter - attenuation.rgbr * texture4D(inscatterSampler, r0, mu0, muS0, nu), 0.0);
                        if (blendRatio > 0.0f)
                        {
                            inscatter = mix(inscatter,
							    (1.f - attenuation.rgbr) * max(texture4D(inscatterSampler, r, MuOriginal, muS, nu), 0.0), 
							    blendRatio);
                        }
                    }
                    else
                    {
                        inscatter = (1.f - attenuation.rgbr) * inscatter;                         
                    }
                }
            }
            // avoids imprecision problems in Mie scattering when sun is below horizon
            inscatter.w *= smoothstep(0.00, 0.02, muS);
            result = max(inscatter.rgb * phaseR + getMie(inscatter) * phaseM, 0.0);
        }
        return result * SUN_INTENSITY;
    }
    
}

module AtmospherePostPassParams
{
	public param vec3 worldSunDir;
    public param vec4 atmosphereParams;
    
    public param Texture2D colorTex;
	public param Texture2D depthTex;
   
    public param Texture2D transmittanceSampler;
    public param Texture2D skyIrradianceSampler;
    public param Texture3D inscatterSampler;

    public param SamplerState linearSampler;
	public param SamplerState nearestSampler;
}

shader AtmospherePostPass targets StandardPipeline
{
	public @MeshVertex vec2 vertPos;
	public @MeshVertex vec2 vertUV;

    [Binding: "0"]
    public using AtmospherePostPassParams;
    
    [Binding: "1"]
    public using ForwardBasePassParams;

    float atmosphericFogScale = atmosphereParams.x;
    
    using atmosphere = Atmosphere();
    
    public vec4 projCoord = vec4(vertPos.xy, 0.0, 1.0);
    
    vec3 hdr(vec3 L)
    {
        float hdrExposure = 0.6;
        L = L * hdrExposure;
        L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
        L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
        L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
        return L;
    }
    
    public out @Fragment vec4 outputColor
    {
		vec4 result;
        float z = depthTex.Sample(nearestSampler, vertUV).r; 
        float x = vertUV.x*2-1;
        float y = vertUV.y*2-1;
        vec4 position = invViewProjTransform * vec4(x, y, z, 1.0f);
        
        vec3 pos = position.xyz / position.w;
        vec3 v = pos-cameraPos;
        float t = length(v);
        v *= (1.0/t);
        if (z == 1.0)
        {
            vec3 sunColor = vec3(step(cos(3.1415926 / 180.0), dot(v, worldSunDir))) * atmosphere.SUN_INTENSITY;
            vec3 extinction = vec3(1.0);
            vec3 av = v;
            av.y = max(0.0, av.y);
            av = normalize(av);
            vec3 inscatter = atmosphere.skyRadiance(cameraPos*0.01 + atmosphere.earthPos, av, worldSunDir, extinction);
            vec3 finalColor = inscatter;
            result = vec4(finalColor, 1.0);
        }
        else
        {
            vec3 colorIn = colorTex.Sample(nearestSampler, vertUV).xyz;
            vec3 extinction;
            vec3 inscatter = atmosphere.AtmosphericScatterSceneGeometry(cameraPos*0.01 +
                 atmosphere.earthPos, 0.1+t*atmosphericFogScale, v, z, worldSunDir, extinction);
            //inscatter = hdr(inscatter);
            vec3 finalColor = colorIn * extinction + inscatter;
            result = vec4(finalColor, 1.0);
        }
		return result;
    }
}