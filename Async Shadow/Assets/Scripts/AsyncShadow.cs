using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

#if UNITY_EDITOR
using UnityEditor;
#endif

public class AsyncShadow : MonoBehaviour
{
    [DllImport("AsyncShadow")]
    static extern bool CheckDevice();
    [DllImport("AsyncShadow")]
    static extern bool CreateResources();
    [DllImport("AsyncShadow")]
    static extern void ReleaseResources();
    [DllImport("AsyncShadow")]
    static extern bool SendMeshData(System.IntPtr _vb, System.IntPtr _ib, int _vertCount, int _indexCount);
    [DllImport("AsyncShadow")]
    static extern bool SendTextureData(System.IntPtr _texture);
    [DllImport("AsyncShadow")]
    static extern bool SendShadowTextureData(System.IntPtr _shadowTexture);
    [DllImport("AsyncShadow")]
    static extern void RenderShadows(bool _multiThread);
    [DllImport("AsyncShadow")]
    static extern void SetObjectTransform(int _index, float[] _pos, float[] _scale, float[] _rot);
    [DllImport("AsyncShadow")]
    static extern void SetObjTextureIndex(int _index, int _texIndex);
    [DllImport("AsyncShadow")]
    static extern void SetLightTransform(float[] _lightPos, float[] _lightDir, float _radius);
    [DllImport("AsyncShadow")]
    static extern void GetLightTransform(float[] _shadowTransform);
    [DllImport("AsyncShadow")]
    static extern double GetShadowRenderTime();
    [DllImport("AsyncShadow")]
    static extern void SetRenderMethod(bool _useIndirect, bool _useBundle);

    public Mesh[] randomMeshes;
    public Texture2D[] randomTextures;
    public Material opaqueMaterial;
    public Material cutoutMaterial;
    public int numberToGenerate;
    public float rangeToGenerate;

    [Header("Light Settings")]
    public bool multiThread = true;
    public bool indirectDrawing = false;
    public bool bundleDrawing = false;
    public int shadowMapSize = 2048;
    public Light mainLight;
    public float directionalShadowRadius = 100.0f;
    [Range(0.0001f, 0.1f)]
    public float shadowBias = 0.005f;

    [System.NonSerialized]
    public RenderTexture shadowMap;
    [System.NonSerialized]
    public Material shadowVis;

    GameObject[] randomObjects;
    Transform[] randomTransforms;
    Transform mainLightTransform;
    Matrix4x4 shadowMatrix = Matrix4x4.identity;

    // data buffer for sending to native
    float[][] objPos;
    float[][] objScale;
    float[][] objRot;
    float[] lightPos = new float[3];
    float[] lightDir = new float[3];
    float[] shadowTransform = new float[16];

    // camera cache
    Camera mainCamera;

    // materials
    Texture2DArray cutoutTextures;

#if UNITY_EDITOR
    // gui debug
    Rect guiRect = new Rect(0, 0, 550.0f, 65.0f);
    Texture2D gTexture;
    GUIStyle guiStyle = new GUIStyle();
    float guiTime = 0.0f;
    double shadowTime = 0.0;
#endif

    void Start ()
    {
        Debug.Log(CheckDevice() ? "D3D12 native device succeed." : "D3D12 native device failed.");
        BuildMaterials();
        RandomGenerateObjects();
        if (!Init())
        {
            enabled = false;
            return;
        }

        mainCamera = Camera.main;

#if UNITY_EDITOR
        gTexture = new Texture2D(1, 1);
        gTexture.SetPixel(0, 0, new Color(0, 0, 0, 0.85f));
        gTexture.Apply();
#endif
    }

    void OnDestroy()
    {
        for (int i = 0; i < numberToGenerate; i++)
        {
            if (randomObjects[i])
            {
                DestroyImmediate(randomObjects[i]);
            }
        }

        if (shadowMap)
        {
            shadowMap.Release();
            DestroyImmediate(shadowMap);
        }

        if(shadowVis)
        {
            DestroyImmediate(shadowVis);
        }

        if(cutoutTextures)
        {
            DestroyImmediate(cutoutTextures);
        }

        Resources.UnloadUnusedAssets();
        ReleaseResources();

#if UNITY_EDITOR
        DestroyImmediate(gTexture);
#endif
    }

    void Update()
    {
        if (mainCamera == null ||
            !mainCamera.enabled)
        {
            mainCamera = Camera.main;
        }
        else
        {
            NativeUpdate();
            Shader.SetGlobalTexture("_AsyncShadow", shadowMap);
            Shader.SetGlobalMatrix("_AsyncShadowMatrix", shadowMatrix);
            Shader.SetGlobalFloat("_ShadowBias", shadowBias);
        }
    }

#if UNITY_EDITOR
    void OnGUI()
    {
        if (guiTime > 1.0f)
        {
            shadowTime = GetShadowRenderTime();
            guiTime = 0.0f;
        }

        guiRect.width = 550.0f * Screen.width / 1920;
        guiRect.height = 85.0f * Screen.height / 1080;

        GUI.DrawTexture(guiRect, gTexture, ScaleMode.StretchToFill, true);
        guiStyle.fontSize = 40 * Screen.width / 1920;
        guiStyle.normal.textColor = Color.white;

        string msg = "Shadow Thread: " + shadowTime.ToString("F4") + " ms.";

        GUI.Label(guiRect, msg, guiStyle);

        guiTime += Time.deltaTime;
    }
#endif

    void BuildMaterials()
    {
        // create 2d tex array for instancing
        cutoutTextures = new Texture2DArray(64, 64, randomTextures.Length, TextureFormat.DXT5, true);
        for (int i = 0; i < randomTextures.Length; i++)
        {
            for (int j = 0; j < randomTextures[i].mipmapCount; j++)
            {
                Graphics.CopyTexture(randomTextures[i], 0, j, cutoutTextures, i, j);
            }
        }
        cutoutTextures.Apply(false, true);

        // set texture 2d array
        cutoutMaterial.SetTexture("_MainTex", cutoutTextures);
    }

    void NativeUpdate()
    {
        SetRenderMethod(indirectDrawing, bundleDrawing);
        UpdateLightTransform();
        RenderShadows(multiThread);
    }

    bool Init()
    {
        if (!CreateResources())
        {
            Debug.LogError("Create resources failed.");
            return false;
        }

        InitMeshData();
        InitTextureData();
        if (!InitShadowMaps())
        {
            return false;
        }
        InitTransform();

        return true;
    }

    void RandomGenerateObjects()
    {
        numberToGenerate = Mathf.Clamp(numberToGenerate, 0, int.MaxValue);
        randomObjects = new GameObject[numberToGenerate];
        randomTransforms = new Transform[numberToGenerate];
        rangeToGenerate = Mathf.Abs(rangeToGenerate);

        objPos = new float[numberToGenerate][];
        objRot = new float[numberToGenerate][];
        objScale = new float[numberToGenerate][];

        MaterialPropertyBlock props = new MaterialPropertyBlock();
        for (int i = 0; i < numberToGenerate; i++)
        {
            // random choose mesh
            randomObjects[i] = new GameObject("Random " + i);
            randomObjects[i].AddComponent<MeshFilter>();

            Mesh chosenMesh = randomMeshes[i % randomMeshes.Length];
            randomObjects[i].GetComponent<MeshFilter>().mesh = chosenMesh;
            randomObjects[i].AddComponent<MeshRenderer>();

            // change to 32 bit index buffer for indirect drawing
            int[] indices = chosenMesh.GetIndices(0);
            randomObjects[i].GetComponent<MeshFilter>().sharedMesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;
            randomObjects[i].GetComponent<MeshFilter>().sharedMesh.SetIndices(indices, MeshTopology.Triangles, 0);

            // random change color
            randomObjects[i].GetComponent<MeshRenderer>().material = (i <= numberToGenerate / 2) ? opaqueMaterial : cutoutMaterial;

            if(i <= numberToGenerate / 2)
            {
                props.SetColor("_Color", new Color(Random.Range(0f, 1f), Random.Range(0f, 1f), Random.Range(0f, 1f), Random.Range(0f, 0.5f)));
                randomObjects[i].GetComponent<MeshRenderer>().SetPropertyBlock(props);
            }
            // generate half cutout object
            else
            {
                // set instance data
                props.SetFloat("_Cutoff", 0.5f);
                props.SetColor("_Color", new Color(Random.Range(0f, 1f), Random.Range(0f, 1f), Random.Range(0f, 1f), Random.Range(0f, 0.5f)));
                props.SetFloat("_TexIndex", i % randomTextures.Length);
                randomObjects[i].GetComponent<MeshRenderer>().SetPropertyBlock(props);
            }

            // random transformation
            randomObjects[i].transform.position = new Vector3(Random.Range(-rangeToGenerate, rangeToGenerate), Random.Range(0, rangeToGenerate), Random.Range(-rangeToGenerate, rangeToGenerate));
            randomObjects[i].transform.localScale = new Vector3(Random.Range(10, 20), Random.Range(10, 20), Random.Range(10, 20));
            randomObjects[i].transform.SetParent(transform);
            randomTransforms[i] = randomObjects[i].transform;

            // cache object transform
            objPos[i] = new float[3];
            objScale[i] = new float[3];
            objRot[i] = new float[4];

            objPos[i][0] = randomTransforms[i].position.x;
            objPos[i][1] = randomTransforms[i].position.y;
            objPos[i][2] = randomTransforms[i].position.z;

            objScale[i][0] = randomTransforms[i].lossyScale.x;
            objScale[i][1] = randomTransforms[i].lossyScale.y;
            objScale[i][2] = randomTransforms[i].lossyScale.z;

            objRot[i][0] = randomTransforms[i].rotation.x;
            objRot[i][1] = randomTransforms[i].rotation.y;
            objRot[i][2] = randomTransforms[i].rotation.z;
            objRot[i][3] = randomTransforms[i].rotation.w;
        }
    }

    void InitMeshData()
    {
        for (int i = 0; i < randomObjects.Length; i++)
        {
            MeshFilter mf = randomObjects[i].GetComponent<MeshFilter>();

            if (!SendMeshData(mf.sharedMesh.GetNativeVertexBufferPtr(0), mf.sharedMesh.GetNativeIndexBufferPtr(), mf.sharedMesh.vertexCount, mf.sharedMesh.GetIndices(0).Length))
            {
                Debug.LogError("Set mesh data failed. " + mf.gameObject.name + " will be ignored.");
            }
        }
    }

    void InitTextureData()
    {
        for (int i = 0; i < randomTextures.Length; i++)
        {
            SendTextureData(randomTextures[i].GetNativeTexturePtr());
        }
    }

    bool InitShadowMaps()
    {
        shadowMap = new RenderTexture(shadowMapSize, shadowMapSize, 24, RenderTextureFormat.Shadowmap, RenderTextureReadWrite.Linear);
        shadowMap.useMipMap = false;
        shadowMap.filterMode = FilterMode.Bilinear;
        shadowMap.wrapMode = TextureWrapMode.Clamp;
        shadowMap.antiAliasing = 1;
        shadowMap.Create();
        shadowMap.name = "Shadow Map";

        if (!SendShadowTextureData(shadowMap.GetNativeDepthBufferPtr()))
        {
            Debug.LogError("Set render texture " + shadowMap.name + " failed.");
            return false;
        }

#if UNITY_EDITOR
        shadowVis = new Material(Shader.Find("AsyncShadow/shadow_vis"));
#endif

        return true;
    }

    void InitTransform()
    {
        mainLightTransform = mainLight.transform;

        for (int i = 0; i < randomObjects.Length; i++)
        {
            // set transform once
            SetObjectTransform(i, objPos[i], objScale[i], objRot[i]);
            SetObjTextureIndex(i, (i > numberToGenerate / 2) ? i % randomTextures.Length : -1);
        }
    }

    void UpdateLightTransform()
    {
        mainLightTransform.Rotate(0.0f, 20.0f * Time.deltaTime, 0.0f, Space.World);

        lightPos[0] = mainLightTransform.position.x;
        lightPos[1] = mainLightTransform.position.y;
        lightPos[2] = mainLightTransform.position.z;

        lightDir[0] = mainLightTransform.forward.x;
        lightDir[1] = mainLightTransform.forward.y;
        lightDir[2] = mainLightTransform.forward.z;

        SetLightTransform(lightPos, lightDir, directionalShadowRadius);
        GetLightTransform(shadowTransform);

        shadowMatrix.m00 = shadowTransform[0];
        shadowMatrix.m01 = shadowTransform[1];
        shadowMatrix.m02 = shadowTransform[2];
        shadowMatrix.m03 = shadowTransform[3];

        shadowMatrix.m10 = shadowTransform[4];
        shadowMatrix.m11 = shadowTransform[5];
        shadowMatrix.m12 = shadowTransform[6];
        shadowMatrix.m13 = shadowTransform[7];

        shadowMatrix.m20 = shadowTransform[8];
        shadowMatrix.m21 = shadowTransform[9];
        shadowMatrix.m22 = shadowTransform[10];
        shadowMatrix.m23 = shadowTransform[11];

        shadowMatrix.m30 = shadowTransform[12];
        shadowMatrix.m31 = shadowTransform[13];
        shadowMatrix.m32 = shadowTransform[14];
        shadowMatrix.m33 = shadowTransform[15];
    }

#if UNITY_EDITOR
    [CustomEditor(typeof(AsyncShadow))]
    public class AsyncShadowEditor : Editor
    {
        public override void OnInspectorGUI()
        {
            DrawDefaultInspector();

            AsyncShadow asyncShadow = target as AsyncShadow;

            // draw debug preview
            if(asyncShadow.shadowMap)
            {
                EditorGUILayout.Space();
                GUILayout.Label("Shadow Map:");

                float flPadding = 5.0f;
                float flWidth = 350.0f;
                float flHeight = flWidth * asyncShadow.shadowMap.height / asyncShadow.shadowMap.width;
                Rect outerRect = GUILayoutUtility.GetRect(flWidth + (flPadding * 2.0f), flHeight + (flPadding * 2.0f), GUILayout.ExpandWidth(false), GUILayout.ExpandHeight(false));
                outerRect.x = Mathf.Max(0.0f, (Screen.width - outerRect.width) * 0.5f - 10.0f);

                EditorGUI.DrawPreviewTexture(outerRect, asyncShadow.shadowMap, asyncShadow.shadowVis);
            }

            Repaint();
        }
    }
#endif
}
