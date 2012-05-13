//package cj.weka;

import cj.CJProxy;
import java.io.*;
import java.util.Enumeration;

/*
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *    Elman.java
 *    Based on code Copyright (C) 2000-2010 University of Waikato, Hamilton, New Zealand
 *
 */

import java.util.Random;
import java.util.Vector;
import java.util.ArrayList;
import java.util.List;
import weka.classifiers.AbstractClassifier;
import weka.classifiers.Classifier;
import weka.classifiers.functions.neural.LinearUnit;
import weka.classifiers.functions.neural.NeuralConnection;
import weka.classifiers.functions.neural.NeuralNode;
import weka.classifiers.functions.neural.SigmoidUnit;
import weka.core.Attribute;
import weka.core.Capabilities;
import weka.core.Capabilities.Capability;
import weka.core.DenseInstance;
import weka.core.Instance;
import weka.core.Instances;
import weka.core.Option;
import weka.core.OptionHandler;
import weka.core.RevisionUtils;
import weka.core.Utils;
import weka.core.WeightedInstancesHandler;
import weka.filters.Filter;
import weka.filters.unsupervised.attribute.NominalToBinary;










public class CJWeka extends AbstractClassifier
    implements CJProxy, OptionHandler, WeightedInstancesHandler
{

    /** for serialization */
    private static final long serialVersionUID = -4393145704384476775L;


    private static Instances ii;


    /** a ZeroR model in case no model can be built from the data */
    private Classifier m_ZeroR;
    /** The training instances. */
    private Instances m_instances;
    /** The current instance running through the network. */
    private Instance m_currentInstance;
    /** A flag to say that it's a numeric class. */
    private boolean m_numeric;
    /** The ranges for all the attributes. */
    private double[] m_attributeRanges;
    /** The base values for all the attributes. */
    private double[] m_attributeBases;
    /** The output units.(only feeds the errors, does no calcs) */
    private NeuralEnd[] m_outputs;
    /** The input units.(only feeds the inputs does no calcs) */
    private NeuralEnd[] m_inputs;
    /** All the nodes that actually comprise the logical neural net. */
    private NeuralConnection[] m_neuralNodes;
    /** The number of classes. */
    private int m_numClasses = 0;
    /** The number of attributes. */
    private int m_numAttributes = 0; //note the number doesn't include the class.
    /** The next id number available for default naming. */
    private int m_nextId;
    /** The number of epochs to train through. */
    private int m_numEpochs;
    /** The number used to seed the random number generator. */
    private int m_randomSeed;
    /** The actual random number generator. */
    private Random m_random;
    /** A flag to state that a nominal to binary filter should be used. */
    private boolean m_useNomToBin;
    /** The actual filter. */
    private NominalToBinary m_nominalToBinaryFilter;
    /** The string that defines the hidden layers */
    private int m_hiddenLayers;
    /** This flag states that the user wants the input values normalized. */
    private boolean m_normalizeAttributes;
    /** This flag states that the user wants the learning rate to decay. */
    private boolean m_decay;
    /** This is the learning rate for the network. */
    private double m_learningRate;
    /** This flag states that the user wants the class to be normalized while
     * processing in the network is done. (the final answer will be in the
     * original range regardless). This option will only be used when the class
     * is numeric. */
    private boolean m_normalizeClass;
    /** this is a sigmoid unit. */
    private SigmoidUnit m_sigmoidUnit;
    /** This is a linear unit. */
    private LinearUnit m_linearUnit;
    /** Keeps the hidden units' values in order to copy these to state nodes */
    double[] m_hiddenValues;
    /** This is the momentum for the network. */
    private double m_momentum;
    /** This flag states that the internal state will be reset after training */
    private boolean m_resetAfterTraining;

    private ArrayList<Attribute> my_attributes;

    private ArrayList<String> classvals;

   /**
    * Default constructor
    */
   public CJWeka() {
       
        m_instances = null;
        m_currentInstance = null;


        m_outputs = new NeuralEnd[0];
        m_inputs = new NeuralEnd[0];
        m_numAttributes = 0;
        m_numClasses = 0;
        m_neuralNodes = new NeuralConnection[0];
        m_nextId = 0;
        m_numeric = false;
        m_random = null;
        m_nominalToBinaryFilter = new NominalToBinary();
        m_sigmoidUnit = new SigmoidUnit();
        m_linearUnit = new LinearUnit();
        //setting all the options to their defaults. To completely change these
        //defaults they will also need to be changed down the bottom in the
        //setoptions function (the text info in the accompanying functions should
        //also be changed to reflect the new defaults
        m_normalizeClass = true;
        m_normalizeAttributes = true;
        m_useNomToBin = true;
        m_numEpochs = 600;
        m_randomSeed = 0;
        m_hiddenLayers = 2;
        m_learningRate = .3;
        m_momentum = 0;
        m_resetAfterTraining = true;
        m_decay = false;

	my_attributes = new ArrayList<Attribute>();

	classvals = new ArrayList<String>();
   }


   public Object start(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("abc");

      // function code goes in here

      return retbuf.toString();
   }



   public Object end(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("def");

      // function code goes in here

      //my_attributes.clear();

      return retbuf.toString();
   }


   public Object addInstance(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("");

      // function code goes in here

      String floatstring = (String)args;

      // convert floatstring to float/double array to instance
      String[] flostr = floatstring.split(" ");
      int nvalues = flostr.length;

      // add instance to ii

      if (my_attributes.isEmpty()) {
	  // create attributes for all instances
	  for (int j=0; j < nvalues-1; j++) {
	      Attribute a = new Attribute(Integer.toString(j));
	      my_attributes.add(a); 
	  }

	  classvals.add("0");
	  classvals.add("1");
	  classvals.add("2");
	  classvals.add("3");
	  classvals.add("4");
	  classvals.add("5");
	  classvals.add("6");
	  classvals.add("7");

	  Attribute cls = new Attribute("class",classvals);

	  my_attributes.add(cls); 

	  ii = new Instances("my_instances", my_attributes, 0);
      }

      ii.setClassIndex(nvalues - 1);

      Instance inst = this.floatstringToInst(floatstring,ii,true);
      ii.add(inst);

      retbuf.append(ii.numClasses()); // return number of Instances in ii
      return retbuf.toString();
   }


   public Object buildModel(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("");

      // function code goes in here

      try {
	  buildClassifier(ii);
      }
      catch (Exception e) {
	  throw e;
      }

      // reset ii

      return retbuf.toString();
   }


   public Object saveModel(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("");

      // function code goes in here


      return retbuf.toString();
   }


   public Object loadModel(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("");

      // function code goes in here


      return retbuf.toString();
   }


   public Object runModel(Object args) throws Exception
   {
      if (!(args instanceof String))
      {
         throw new RuntimeException("Invalid type for execute");
      }

      StringBuffer retbuf = new StringBuffer("");

      // function code goes in here
      String floatstring = (String)args;

      double res[];
      Instance inst = this.floatstringToInst(floatstring,ii,false);

      try {
	  res = distributionForInstance(inst);
      }

      catch (Exception e) {
	  throw e;
      }

      // append res value to retbuf

      int nvals = res.length;

      for (int j=0; j<nvals; j++) {
	  retbuf.append(res[j]+" ");
      }

      return retbuf.toString();
   }



    ///////////////////////////////////////////////////////////

    /** Convert a sting of floats separated by spaces into an Instance
     */
    private Instance floatstringToInst(String floatvalues, Instances ii, boolean hasClass) {
	String[] flostr = floatvalues.split(" ");
	int nvals = flostr.length;
	Instance i = new DenseInstance(nvals);
	int j;

	if (hasClass) nvals--;

	for (j=0; j<nvals; j++) {
	    Float f = new Float(flostr[j]);
	    i.setValue(j, f);
	}

	i.setDataset(ii);

	if (hasClass) {
	    Attribute clsAttrib = ii.classAttribute();
	    clsAttrib.addStringValue(flostr[j]);
	    i.setValue(clsAttrib, flostr[j]);
	}

	return i;
    }


    ////////////////////////////////////////////////////////////


    // weka methods

    /**
     * This inner class is used to connect the nodes in the network up to
     * the data that they are classifying, Note that objects of this class are
     * only suitable to go on the attribute side or class side of the network
     * and not both.
     */
    protected class NeuralEnd
            extends NeuralConnection {

        /** for serialization */
        static final long serialVersionUID = 7305185603191183338L;
        /**
         * the value that represents the instance value this node represents.
         * For an input it is the attribute number, for an output, if nominal
         * it is the class value.
         */
        private int m_link;
        /** True if node is an input */
        private boolean m_input;
        /** True if node is an output. */
        private boolean m_output;

        /**
         * Constructor
         */
        public NeuralEnd(String id) {
            super(id);

            m_link = 0;
            m_input = true;
            m_output = false;

        }

        /**
         * Call this to get the output value of this unit.
         * @param calculate True if the value should be calculated if it hasn't
         * been already.
         * @return The output value, or NaN, if the value has not been calculated.
         */
        public double outputValue(boolean calculate) {

            if (Double.isNaN(m_unitValue) && calculate) {
                if (m_input) {
                    if (m_currentInstance.isMissing(m_link)) {
                        m_unitValue = 0;
                    } else {

                        m_unitValue = m_currentInstance.value(m_link);
                    }
                } else if (m_output) {
                    //node is an output.
                    m_unitValue = 0;
                    for (int noa = 0; noa < m_numInputs; noa++) {
                        m_unitValue += m_inputList[noa].outputValue(true);
                    }
                    if (m_numeric && m_normalizeClass) {
                        //then scale the value;
                        //this scales linearly from between -1 and 1
                        m_unitValue = m_unitValue
                                * m_attributeRanges[m_instances.classIndex()]
                                + m_attributeBases[m_instances.classIndex()];
                    }
                } else {
                    // node is feedback
                    m_unitValue = m_hiddenValues[m_link];
                    if (Double.isNaN(m_unitValue)) {
                        m_unitValue = 0.5;
                    }
                }
            }
            return m_unitValue;
        }

        /**
         * Call this to get the error value of this unit, which in this case is
         * the difference between the predicted class, and the actual class.
         * @param calculate True if the value should be calculated if it hasn't
         * been already.
         * @return The error value, or NaN, if the value has not been calculated.
         */
        public double errorValue(boolean calculate) {

            if (!Double.isNaN(m_unitValue) && Double.isNaN(m_unitError) && calculate) {

                if (!m_output) {
                    m_unitError = 0;

                    for (int noa = 0; noa < m_numOutputs; noa++) {
                        m_unitError += m_outputList[noa].errorValue(true);
                    }
                } else {
                    if (m_currentInstance.classIsMissing()) {
                        m_unitError = 0.0;
                    } else if (m_instances.classAttribute().isNominal()) {
                        if (m_currentInstance.classValue() == m_link) {
                            m_unitError = 1 - m_unitValue;
                        } else {
                            m_unitError = 0 - m_unitValue;
                        }
                    } else if (m_numeric) {

                        if (m_normalizeClass) {
                            if (m_attributeRanges[m_instances.classIndex()] == 0) {
                                m_unitError = 0;
                            } else {
                                m_unitError = (m_currentInstance.classValue() - m_unitValue)
                                        / m_attributeRanges[m_instances.classIndex()];
                            }
                        } else {
                            m_unitError = m_currentInstance.classValue() - m_unitValue;
                        }
                    }
                }
            }

            return m_unitError;
        }
    
        /**
         * Call this to reset the value and error for this unit, ready for the next
         * run. This will also call the reset function of all units that are
         * connected as inputs to this one.
         * This is also the time that the update for the listeners will be
         * performed.
         */
        public void reset() {

            if (!Double.isNaN(m_unitValue) || !Double.isNaN(m_unitError)) {
                m_unitValue = Double.NaN;
                m_unitError = Double.NaN;
                m_weightsUpdated = false;
                for (int noa = 0; noa < m_numInputs; noa++) {
                    m_inputList[noa].reset();
                }
            }
        }

        /**
         * Call this to have the connection save the current
         * weights.
         */
        public void saveWeights() {
            for (int i = 0; i < m_numInputs; i++) {
                m_inputList[i].saveWeights();
            }
        }

        /**
         * Call this to have the connection restore from the saved
         * weights.
         */
        public void restoreWeights() {
            for (int i = 0; i < m_numInputs; i++) {
                m_inputList[i].restoreWeights();
            }
        }

        /**
         * Call this function to set What this end unit represents.
         * @param input True if this unit is used for entering an attribute,
         * False if it's used for determining a class value.
         * @param val The attribute number or class type that this unit represents.
         * (for nominal attributes).
         */
        public void setLink(boolean input, boolean output, int val) throws Exception {
            m_input = input;
            m_output = output;

            if (input) {
                m_type = PURE_INPUT;
            } else if (output) {
                m_type = PURE_OUTPUT;
            } else {
		m_type = FEEDBACK;
            }
            if (val < 0 || (input && val > m_instances.numAttributes())
                    || (output && m_instances.classAttribute().isNominal()
                    && val > m_instances.classAttribute().numValues())) {
                m_link = 0;
            } else {
                m_link = val;
            }
        }

        /**
         * @return link for this node.
         */
        public int getLink() {
            return m_link;
        }

        /**
         * Returns the revision string.
         *
         * @return		the revision
         */
        public String getRevision() {
            return RevisionUtils.extract("$Revision: 1 $");
        }
    }

    /**
     * @param d True if the learning rate should decay.
     */
    public void setDecay(boolean d) {
        m_decay = d;
    }

    /**
     * @return the flag for having the learning rate decay.
     */
    public boolean getDecay() {
        return m_decay;
    }

    /**
     * @param c True if the class should be normalized (the class will only ever
     * be normalized if it is numeric). (Normalization puts the range between
     * -1 - 1).
     */
    public void setNormalizeNumericClass(boolean c) {
        m_normalizeClass = c;
    }

    /**
     * @return The flag for normalizing a numeric class.
     */
    public boolean getNormalizeNumericClass() {
        return m_normalizeClass;
    }

    /**
     * @param d True if the internal state will be reset after training.
     */
    public void setResetAfterTraining(boolean r) {
        m_resetAfterTraining = r;
    }

    /**
     * @return The flag for reseting the internal state after training.
     */
    public boolean getResetAfterTraining() {
        return m_resetAfterTraining;
    }

    /**
     * @param a True if the attributes should be normalized (even nominal
     * attributes will get normalized here) (range goes between -1 - 1).
     */
    public void setNormalizeAttributes(boolean a) {
        m_normalizeAttributes = a;
    }

    /**
     * @return The flag for normalizing attributes.
     */
    public boolean getNormalizeAttributes() {
        return m_normalizeAttributes;
    }

    /**
     * @param f True if a nominalToBinary filter should be used on the
     * data.
     */
    public void setNominalToBinaryFilter(boolean f) {
        m_useNomToBin = f;
    }

    /**
     * @return The flag for nominal to binary filter use.
     */
    public boolean getNominalToBinaryFilter() {
        return m_useNomToBin;
    }

    /**
     * This seeds the random number generator, that is used when a random
     * number is needed for the network.
     * @param l The seed.
     */
    public void setSeed(int l) {
        if (l >= 0) {
            m_randomSeed = l;
        }
    }

    /**
     * @return The seed for the random number generator.
     */
    public int getSeed() {
        return m_randomSeed;
    }

    /**
     * The learning rate can be set using this command.
     * NOTE That this is a static variable so it affect all networks that are
     * running.
     * Must be greater than 0 and no more than 1.
     * @param l The New learning rate.
     */
    public void setLearningRate(double l) {
        if (l > 0 && l <= 1) {
            m_learningRate = l;
        }
    }

    /**
     * @return The learning rate for the nodes.
     */
    public double getLearningRate() {
        return m_learningRate;
    }

    /**
     * The momentum can be set using this command.
     * THE same conditions apply to this as to the learning rate.
     * @param m The new Momentum.
     */
    public void setMomentum(double m) {
        if (m >= 0 && m <= 1) {
            m_momentum = m;
        }
    }

    /**
     * @return The momentum for the nodes.
     */
    public double getMomentum() {
        return m_momentum;
    }

    /**
     * This will set what the hidden layers are made up of when auto build is
     * enabled. Note to have no hidden units, just put a single 0, Any more
     * 0's will indicate that the string is badly formed and make it unaccepted.
     * Negative numbers, and floats will do the same. There are also some
     * wildcards. These are 'a' = (number of attributes + number of classes) / 2,
     * 'i' = number of attributes, 'o' = number of classes, and 't' = number of
     * attributes + number of classes.
     * @param h A string with a comma seperated list of numbers. Each number is
     * the number of nodes to be on a hidden layer.
     */
    public void setHiddenLayers(int h) {
        if (h > 0) {
            m_hiddenLayers = h;
        }
    }

    /**
     * @return A string representing the hidden layers, each number is the number
     * of nodes on a hidden layer.
     */
    public int getHiddenLayers() {
        return m_hiddenLayers;
    }

    /**
     * Set the number of training epochs to perform.
     * Must be greater than 0.
     * @param n The number of epochs to train through.
     */
    public void setTrainingTime(int n) {
        if (n > 0) {
            m_numEpochs = n;
        }
    }

    /**
     * @return The number of epochs to train through.
     */
    public int getTrainingTime() {
        return m_numEpochs;
    }

    /**
     * Call this function to place a node into the network list.
     * @param n The node to place in the list.
     */
    private void addNode(NeuralConnection n) {

        NeuralConnection[] temp1 = new NeuralConnection[m_neuralNodes.length + 1];
        for (int noa = 0; noa < m_neuralNodes.length; noa++) {
            temp1[noa] = m_neuralNodes[noa];
        }

        temp1[temp1.length - 1] = n;
        m_neuralNodes = temp1;
    }

    /**
     * Call this function to remove the passed node from the list.
     * This will only remove the node if it is in the neuralnodes list.
     * @param n The neuralConnection to remove.
     * @return True if removed false if not (because it wasn't there).
     */
    private boolean removeNode(NeuralConnection n) {
        NeuralConnection[] temp1 = new NeuralConnection[m_neuralNodes.length - 1];
        int skip = 0;
        for (int noa = 0; noa < m_neuralNodes.length; noa++) {
            if (n == m_neuralNodes[noa]) {
                skip++;
            } else if (!((noa - skip) >= temp1.length)) {
                temp1[noa - skip] = m_neuralNodes[noa];
            } else {
                return false;
            }
        }
        m_neuralNodes = temp1;
        return true;
    }

    /**
     * This function sets what the m_numeric flag to represent the passed class
     * it also performs the normalization of the attributes if applicable
     * and sets up the info to normalize the class. (note that regardless of
     * the options it will fill an array with the range and base, set to
     * normalize all attributes and the class to be between -1 and 1)
     * @param inst the instances.
     * @return The modified instances. This needs to be done. If the attributes
     * are normalized then deep copies will be made of all the instances which
     * will need to be passed back out.
     */
    private Instances setClassType(Instances inst) throws Exception {
        if (inst != null) {
            // x bounds
            double min = Double.POSITIVE_INFINITY;
            double max = Double.NEGATIVE_INFINITY;
            double value;
            m_attributeRanges = new double[inst.numAttributes()];
            m_attributeBases = new double[inst.numAttributes()];
            for (int noa = 0; noa < inst.numAttributes(); noa++) {
                min = Double.POSITIVE_INFINITY;
                max = Double.NEGATIVE_INFINITY;
                for (int i = 0; i < inst.numInstances(); i++) {
                    if (!inst.instance(i).isMissing(noa)) {
                        value = inst.instance(i).value(noa);
                        if (value < min) {
                            min = value;
                        }
                        if (value > max) {
                            max = value;
                        }
                    }
                }

                m_attributeRanges[noa] = (max - min) / 2;
                m_attributeBases[noa] = (max + min) / 2;
                if (noa != inst.classIndex() && m_normalizeAttributes) {
                    for (int i = 0; i < inst.numInstances(); i++) {
                        if (m_attributeRanges[noa] != 0) {
                            inst.instance(i).setValue(noa, (inst.instance(i).value(noa)
                                    - m_attributeBases[noa])
                                    / m_attributeRanges[noa]);
                        } else {
                            inst.instance(i).setValue(noa, inst.instance(i).value(noa)
                                    - m_attributeBases[noa]);
                        }
                    }
                }
            }
            if (inst.classAttribute().isNumeric()) {
                m_numeric = true;
            } else {
                m_numeric = false;
            }
        }
        return inst;
    }


    /**
     * This will cause the output values of all the nodes to be calculated.
     * Note that the m_currentInstance is used to calculate these values.
     */
    private void calculateOutputs() {
        for (int noc = 0; noc < m_numClasses; noc++) {
            //get the values.
            m_outputs[noc].outputValue(true);
        }
    }

    /**
     * This will cause the error values to be calculated for all nodes.
     * Note that the m_currentInstance is used to calculate these values.
     * Also the output values should have been calculated first.
     * @return The squared error.
     */
    private double calculateErrors() throws Exception {
        double ret = 0, temp = 0;
        for (int noc = 0; noc < m_numAttributes + m_hiddenLayers; noc++) {
            //get the errors.
            m_inputs[noc].errorValue(true);

        }
        for (int noc = 0; noc < m_numClasses; noc++) {
            temp = m_outputs[noc].errorValue(false);
            ret += temp * temp;
        }
        return ret;

    }

    /**
     * This will cause the weight values to be updated based on the learning
     * rate, momentum and the errors that have been calculated for each node.
     * @param l The learning rate to update with.
     * @param m The momentum to update with.
     */
    private void updateNetworkWeights(double l, double m) {
        for (int noc = 0; noc < m_numClasses; noc++) {
            //update weights
            m_outputs[noc].updateWeights(l, m);
        }

    }

    /**
     * This creates the required input units.
     */
    private void setupInputs() throws Exception {
        m_inputs = new NeuralEnd[m_numAttributes + m_hiddenLayers];
        int now = 0;
        for (int noa = 0; noa < m_numAttributes + 1; noa++) {
            if (m_instances.classIndex() != noa) {
                m_inputs[noa - now] = new NeuralEnd(m_instances.attribute(noa).name());

                m_inputs[noa - now].setLink(true, false, noa);
            } else {
                now = 1;
            }
        }
        for (int noa = 0; noa < m_hiddenLayers; noa++) {
            m_inputs[m_numAttributes + noa] = new NeuralEnd("s" + noa);
            m_inputs[m_numAttributes + noa].setLink(false, false, noa);
        }
    }

    /**
     * This creates the required output units.
     */
    private void setupOutputs() throws Exception {

        m_outputs = new NeuralEnd[m_numClasses];
        for (int noa = 0; noa < m_numClasses; noa++) {
            if (m_numeric) {
                m_outputs[noa] = new NeuralEnd(m_instances.classAttribute().name());
            } else {
                m_outputs[noa] = new NeuralEnd(m_instances.classAttribute().name() + m_instances.classAttribute().value(noa));
            }

            m_outputs[noa].setLink(false, true, noa);
            NeuralNode temp = new NeuralNode("o" + m_nextId, m_random,
                    m_sigmoidUnit);
            m_nextId++;
            addNode(temp);
            NeuralConnection.connect(temp, m_outputs[noa]);
        }

    }

    /**
     * Call this function to automatically generate the hidden units
     */
    private void setupHiddenLayer() {
        for (int nob = 0; nob < m_hiddenLayers; nob++) {
            NeuralNode temp = new NeuralNode("h" + m_nextId, m_random,
                    m_sigmoidUnit);
            m_nextId++;
            addNode(temp);
        }

        for (int noa = 0; noa < m_numAttributes + m_hiddenLayers; noa++) {
            for (int nob = m_numClasses; nob < m_numClasses + m_hiddenLayers; nob++) {
                NeuralConnection.connect(m_inputs[noa], m_neuralNodes[nob]);
            }
        }
        for (int noa = m_numClasses; noa < m_neuralNodes.length;
                noa++) {
            for (int nob = 0; nob < m_numClasses; nob++) {
                NeuralConnection.connect(m_neuralNodes[noa], m_neuralNodes[nob]);
            }
        }


    }

    /**
     * This will go through all the nodes and check if they are connected
     * to a pure output unit. If so they will be set to be linear units.
     * If not they will be set to be sigmoid units.
     */
    private void setEndsToLinear() {
        for (int noa = 0; noa < m_neuralNodes.length; noa++) {
            if ((m_neuralNodes[noa].getType() & NeuralConnection.OUTPUT)
                    == NeuralConnection.OUTPUT) {
                ((NeuralNode) m_neuralNodes[noa]).setMethod(m_linearUnit);
            } else {
                ((NeuralNode) m_neuralNodes[noa]).setMethod(m_sigmoidUnit);
            }
        }
    }

    /**
     * Returns default capabilities of the classifier.
     *
     * @return      the capabilities of this classifier
     */
    public Capabilities getCapabilities() {
        Capabilities result = super.getCapabilities();
        result.disableAll();

        // attributes
        result.enable(Capability.NOMINAL_ATTRIBUTES);
        result.enable(Capability.NUMERIC_ATTRIBUTES);
        result.enable(Capability.DATE_ATTRIBUTES);
        result.enable(Capability.MISSING_VALUES);

        // class
        result.enable(Capability.NOMINAL_CLASS);
        result.enable(Capability.NUMERIC_CLASS);
        result.enable(Capability.DATE_CLASS);
        result.enable(Capability.MISSING_CLASS_VALUES);

        return result;
    }



    /**
     * this will reset all the nodes in the network.
     */
    private void resetNetwork() {
        for (int noc = 0; noc < m_numClasses; noc++) {
            m_outputs[noc].reset();
        }
    }




    private void saveValues() {
        for (int noa = m_numClasses; noa < m_neuralNodes.length; noa++) {
            m_hiddenValues[noa - m_numClasses] = m_neuralNodes[noa].outputValue(false);
        }
    }

    
    public void buildClassifier(Instances i) throws Exception {

        // can classifier handle the data?
        getCapabilities().testWithFail(i);

        // remove instances with missing class
        i = new Instances(i);
        i.deleteWithMissingClass();

        // only class? -> build ZeroR model
        if (i.numAttributes() == 1) {
            System.err.println(
                    "Cannot build model (only class attribute present in data!), "
                    + "using ZeroR model instead!");
            m_ZeroR = new weka.classifiers.rules.ZeroR();
            m_ZeroR.buildClassifier(i);
            return;
        } else {
            m_ZeroR = null;
        }

        m_instances = null;
        m_currentInstance = null;

        m_outputs = new NeuralEnd[0];
        m_inputs = new NeuralEnd[0];
        m_numAttributes = 0;
        m_numClasses = 0;
        m_neuralNodes = new NeuralConnection[0];

        m_nextId = 0;
        m_instances = new Instances(i);
        m_random = new Random(m_randomSeed);

        if (m_useNomToBin) {
            m_nominalToBinaryFilter = new NominalToBinary();
            m_nominalToBinaryFilter.setInputFormat(m_instances);
            m_instances = Filter.useFilter(m_instances, m_nominalToBinaryFilter);
        }
        m_numAttributes = m_instances.numAttributes() - 1;
        m_numClasses = m_instances.numClasses();


        setClassType(m_instances);

        setupInputs();
        setupOutputs();
        setupHiddenLayer();


        //For silly situations in which the network gets accepted before training
        //commenses
        if (m_numeric) {
            setEndsToLinear();
        }

        //connections done.
        double right = 0;
        double tempRate;
        double totalWeight = 0;

        m_hiddenValues = new double[m_hiddenLayers];
        resetNetwork();
        saveValues();
        for (int noa = 1; noa < m_numEpochs + 1; noa++) {
//            System.out.println(noa);
            resetNetwork();
            totalWeight = 0;
            right = 0;
            for (int nob = 0; nob < m_instances.numInstances(); nob++) {
                m_currentInstance = m_instances.instance(nob);
                if (!m_currentInstance.classIsMissing()) {
                    totalWeight += m_currentInstance.weight();

                    //this is where the network updating (and training occurs, for the
                    //training set
                    resetNetwork();
                    calculateOutputs();
                    tempRate = m_learningRate * m_currentInstance.weight();
                    if (m_decay) {
                        tempRate /= noa;
                    }

                    right += (calculateErrors() / m_instances.numClasses())
                            * m_currentInstance.weight();
                    updateNetworkWeights(tempRate, m_momentum);
                    saveValues();
                }
            }
            right /= totalWeight;
            if (Double.isInfinite(right) || Double.isNaN(right)) {
                m_instances = null;
                throw new Exception("Network cannot train. Try restarting with a"
                        + " smaller learning rate.");
            }
//            System.out.println(noa+ ": " +right);
        }
        resetNetwork();
        if (m_resetAfterTraining) {
            // in that point it saves Double.NaN
            saveValues();
        }


    }



    /**
     * Call this function to predict the class of an instance once a
     * classification model has been built with the buildClassifier call.
     * @param i The instance to classify.
     * @return A double array filled with the probabilities of each class type.
     * @throws Exception if can't classify instance.
     */
    public double[] distributionForInstance(Instance i) throws Exception {
        // default model?
        if (m_ZeroR != null) {
            return m_ZeroR.distributionForInstance(i);
        }

        if (m_useNomToBin) {
            m_nominalToBinaryFilter.input(i);
            m_currentInstance = m_nominalToBinaryFilter.output();
        } else {
            m_currentInstance = i;
        }

        if (m_normalizeAttributes) {
            for (int noa = 0; noa < m_instances.numAttributes(); noa++) {
                if (noa != m_instances.classIndex()) {
                    if (m_attributeRanges[noa] != 0) {
                        m_currentInstance.setValue(noa, (m_currentInstance.value(noa)
                                - m_attributeBases[noa])
                                / m_attributeRanges[noa]);
                    } else {
                        m_currentInstance.setValue(noa, m_currentInstance.value(noa)
                                - m_attributeBases[noa]);
                    }
                }
            }
        }
        resetNetwork();

        //since all the output values are needed.
        //They are calculated manually here and the values collected.
        double[] theArray = new double[m_numClasses];
        for (int noa = 0; noa < m_numClasses; noa++) {
            theArray[noa] = m_outputs[noa].outputValue(true);
        }
        saveValues();
        if (m_instances.classAttribute().isNumeric()) {
            return theArray;
        }

        //now normalize the array
        double count = 0;
        for (int noa = 0; noa < m_numClasses; noa++) {
            count += theArray[noa];
        }
        if (count <= 0) {
            return null;
        }
        for (int noa = 0; noa < m_numClasses; noa++) {
            theArray[noa] /= count;
        }
        return theArray;
    }






}

