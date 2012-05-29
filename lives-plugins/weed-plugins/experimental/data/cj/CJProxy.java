package cj;

/**
 * CJProxy is the Java side of the C-to-Java JNI interface.
 * Classes that implement CJProxy implement the command pattern
 */
public interface CJProxy
{
   public Object start(Object args) throws Exception;
   public Object end(Object args) throws Exception;
   public Object addInstance(Object args) throws Exception;
   public Object buildModel(Object args) throws Exception;
   public Object saveModel(Object args) throws Exception;
   public Object loadModel(Object args) throws Exception;
   public Object runModel(Object args) throws Exception;
   public Object resetModel(Object args) throws Exception;
}

