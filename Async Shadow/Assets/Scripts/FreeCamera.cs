using UnityEngine;
using System.IO;

public class FreeCamera : MonoBehaviour {

    public float moveSpeed;

	// Use this for initialization
	void Start ()
    {

    }

    // Update is called once per frame
    void Update()
    {
        if (Input.GetMouseButton(0))
        {
            if (Input.GetAxis("Mouse X") != 0)
            {
                transform.Rotate(0.0f, 100.0f * Time.deltaTime * Input.GetAxis("Mouse X"), 0.0f, Space.World);
            }

            if (Input.GetAxis("Mouse Y") != 0)
            {
                transform.Rotate(100.0f * Time.deltaTime * (-Input.GetAxis("Mouse Y")), 0.0f, 0.0f);
            }
        }

        if (Input.GetKey("w"))
        {
            transform.Translate(0.0f, 0.0f, moveSpeed * Time.deltaTime);
        }

        if (Input.GetKey("s"))
        {
            transform.Translate(0.0f, 0.0f, -moveSpeed * Time.deltaTime);
        }

        if (Input.GetKey("a"))
        {
            transform.Translate(-moveSpeed * Time.deltaTime, 0.0f, 0.0f);
        }

        if (Input.GetKey("d"))
        {
            transform.Translate(moveSpeed * Time.deltaTime, 0.0f, 0.0f);
        }
    }
}
