using System.Collections;
using System.Collections.Generic;
using UnityEngine;
//using namespace Microsoft.MixedReality.OpenXR;
#if WINDOWS_UWP


#endif
/*
TODO: REFACTOR THIS
for some reason, even if i put a package dependency to com.microsoft.mixedreality.openxr, 
the compiler still says it can't find the OpenXR namespace.

so my shitty hack for now is to have this here, and then in the project space, have a SpatHelper that actually slaps the SpatialCoordinate system in place

This is a shitty singletone.  Don't try this at home, kids.
*/
public class WMCoordinateSystem : MonoBehaviour
{
    private static object instance;
    /// <summary>
    /// 
    /// </summary>
    /// <returns>Windows.Perception.SpatialCoordinateSystem</returns>
    public static object GetInstance(){
        return instance;
    }
    public static void SetInstance(object o){
        instance = o;
    }

    
    // Start is called before the first frame update
    void Awake()
    {
        
#if WINDOWS_UWP
        //instance=Microsoft.MixedReality.OpenXR.PerceptionInterop.GetSceneCoordinateSystem(UnityEngine.Pose.identity) as SpatialCoordinateSystem;
        //instance = Windows.Perception.Spatial.SpatialLocator.GetDefault().CreateStationaryFrameOfReferenceAtCurrentLocation().CoordinateSystem;
#else
        instance = null;
#endif
    }
    
    
    void OnDestroy(){
        instance = null;
    }


}
