
#include "protoPipeJni.h"
#include "protoPipe.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    createProtoPipe
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_createProtoPipe
  (JNIEnv* env, jobject obj)
{
    ProtoPipe* thePipe = new ProtoPipe(ProtoPipe::MESSAGE);
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    env->SetLongField(obj, fid, (jlong)thePipe);
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    listen
 * Signature: (Ljava/lang/String;)Ljava/lang/Boolean;
 */
JNIEXPORT jboolean JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_listen
  (JNIEnv* env, jobject obj, jstring pipeName)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    const char* namePtr = env->GetStringUTFChars(pipeName, NULL);
    bool result = thePipe->Listen(namePtr);
    env->ReleaseStringUTFChars(pipeName, namePtr);
    return (jboolean)result;
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    read
 * Signature: ([BII)I
 */
JNIEXPORT jint JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_read
  (JNIEnv* env, jobject obj, jbyteArray buffer, jint offset, jint length)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    unsigned int numBytes = (unsigned int) length;
    jbyte* ptr = env->GetByteArrayElements(buffer, NULL);
    if (thePipe->Recv((char*)ptr, numBytes))
    {
        // Release the jbyte* to copy them back to the jbyteArray
        env->ReleaseByteArrayElements(buffer, ptr, 0);
        return (jint)numBytes;
    }
    else
    {
        // Release the jbyte* to copy them back to the jbyteArray
        env->ReleaseByteArrayElements(buffer, ptr, 0);
        return (jint)-1;
    }
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    connect
 * Signature: (Ljava/lang/String;)Ljava/lang/Boolean;
 */
JNIEXPORT jboolean JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_connect
  (JNIEnv* env, jobject obj, jstring pipeName)
 {
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    const char* namePtr = env->GetStringUTFChars(pipeName, NULL);
    bool result = thePipe->Connect(namePtr);
    env->ReleaseStringUTFChars(pipeName, namePtr);
    return (jboolean)result;
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    write
 * Signature: ([BII)V
 */
JNIEXPORT void JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_write
  (JNIEnv* env, jobject obj, jbyteArray buffer, jint offset, jint length)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    unsigned int numBytes = (unsigned int)length;
    jbyte* ptr = env->GetByteArrayElements(buffer, NULL);
    thePipe->Send((char*)ptr, numBytes);
    env->ReleaseByteArrayElements(buffer, ptr, 0);
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_close
  (JNIEnv* env, jobject obj)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    thePipe->Close();
}

/*
 * Class:     mil_navy_nrl_protolib_ProtoPipe
 * Method:    doFinalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mil_navy_nrl_protolib_ProtoPipe_doFinalize
  (JNIEnv* env, jobject obj)
{
    jfieldID fid = env->GetFieldID(env->GetObjectClass(obj), "handle", "J");
    ProtoPipe* thePipe = (ProtoPipe*)env->GetLongField(obj, fid);
    if (NULL != thePipe)
    {
        if (thePipe->IsOpen()) thePipe->Close();
        delete thePipe;
        env->SetLongField(obj, fid, (jlong)0);
    }
}

#ifdef __cplusplus
}
#endif
