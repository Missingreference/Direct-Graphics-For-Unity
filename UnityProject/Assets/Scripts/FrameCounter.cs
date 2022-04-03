using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using TMPro;

public class FrameCounter : MonoBehaviour
{
    TextMeshProUGUI textMesh;
    private void Awake()
    {
        textMesh = GetComponent<TextMeshProUGUI>();
    }

    // Update is called once per frame
    void Update()
    {
        if(Time.frameCount % 2 == 0)
        {
            textMesh.text = "x " + Time.frameCount.ToString();
        }
        else
        {
            textMesh.text = "+ " + Time.frameCount.ToString();
        }
    }
}
