using System.Collections;
using System.Collections.Generic;
using UnityEngine;
#if !UNITY_EDITOR && UNITY_WSA
using Windows.Perception;
using Windows.Perception.Spatial;
#endif
public class SpatHelper : MonoBehaviour
{
    // Start is called before the first frame update
    void Start()
    {

#if !UNITY_EDITOR && UNITY_WSA
            WMCoordinateSystem.SetInstance(Microsoft.MixedReality.OpenXR.PerceptionInterop.GetSceneCoordinateSystem(UnityEngine.Pose.identity));// as SpatialCoordinateSystem;    
#endif
    }


}
