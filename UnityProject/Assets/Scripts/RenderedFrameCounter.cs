using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using TMPro;

public class RenderedFrameCounter : MonoBehaviour
{
    TextMeshProUGUI textMesh;
    private void Awake()
    {
        textMesh = GetComponent<TextMeshProUGUI>();
    }

    // Update is called once per frame
    void Update()
    {
        if(Time.renderedFrameCount % 2 == 0)
        {
            textMesh.text = "x " + Time.renderedFrameCount.ToString();
        }
        else
        {
            textMesh.text = "+ " + Time.renderedFrameCount.ToString();
        }
    }
}
