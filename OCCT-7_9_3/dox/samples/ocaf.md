# OCAF: Usage Tutorial {#samples__ocaf}

## Getting Started

At the beginning of your development, you first define an application class by inheriting from the Application abstract class.
You only have to create and determine the resources of the application for specifying the format of your documents (you generally use the standard one) and their file extension.

Then, you design the application data model by organizing attributes you choose among those provided with OCAF.
You can specialize these attributes using the User attribute. For example, if you need a reflection coefficient,
you aggregate a User attribute identified as a reflection coefficient
with a Real attribute containing the value of the coefficient (as such, you don't define a new class).

If you need application specific data not provided with OCAF, for example, to incorporate a finite element model in the data structure,
you define a new attribute class containing the mesh, and you include its persistent homologue in a new file format.

Once you have implemented the commands which create and modify the data structure according to your specification, OCAF provides you, without any additional programming:

- Persistent reference to any data, including geometric elements - several documents can be linked with such reference;
- Document-View association;
- Ready-to-use functions such as:
  - Undo-redo;
  - Save and open application data.

Finally, you develop the application's graphical user interface using the toolkit of your choice, for example:

- KDE Qt or GNOME GTK+ on Linux;
- Microsoft Foundation Classes (MFC) on Windows Motif on Sun;
- Other commercial products such as Ilog Views.

You can also implement the user interface in the Java language using the Swing-based Java Application Desktop component (JAD) provided with OCAF.

## An example of OCAF usage

To create a useful OCAF-based application, it is necessary to redefine two deferred methods: *Formats* and *ResourcesName*

In the *Formats* method, add the format of the documents, which need to be read by the application and may have been built in other applications.

For example:

In the *ResourcesName* method, you only define the name of the resource file.
This file contains several definitions for the saving and opening mechanisms associated with each format and calling of the plug-in file.

To obtain the saving and opening mechanisms, it is necessary to set two environment variables: *CSF_PluginDefaults*, which defines the path of the plug-in file,
and *CSF_ResourcesDefault*, which defines the resource file:

The plugin and the resource files of the application will be located in *myDirector*.
The name of the plugin file must be *Plugin*.

### Resource File

The resource file describes the documents (type and extension) and the type of data that the application can manipulate
by identifying the storage and retrieval drivers appropriate for this data.

Each driver is unique and identified by a GUID generated, for example, with the *uuidgen* tool in Windows.

Five drivers are required to use all standard attributes provided within OCAF:

- the schema driver (ad696002-5b34-11d1-b5ba-00a0c9064368)
- the document storage driver (ad696000-5b34-11d1-b5ba-00a0c9064368)
- the document retrieval driver (ad696001-5b34-11d1-b5ba-00a0c9064368)
- the attribute storage driver (47b0b826-d931-11d1-b5da-00a0c9064368)
- the attribute retrieval driver (47b0b827-d931-11d1-b5da-00a0c9064368)

These drivers are provided as plug-ins and are located in the *PappStdPlugin* library.

For example, this is a resource file, which declares a new model document OCAF-MyApplication:

### Plugin File

The plugin file describes the list of required plug-ins to run the application and the libraries in which plug-ins are located.

You need at least the *FWOSPlugin* and the plug-in drivers to run an OCAF application.

The syntax of each item is *Identification.Location Library_Name*, where:

- Identification is GUID.
- Location defines the location of the Identification (where its definition is found).
- Library_Name is the name (and path to) the library, where the plug-in is located.

For example, this is a Plugin file:

## Implementation of Attribute Transformation in a HXX file



## Implementation of Attribute Transformation in a CPP file



## Implementation of typical actions with standard OCAF attributes.

There are four sample files provided in the directory 'OpenCasCade/ros/samples/ocafsamples'.
They present typical actions with OCAF services (mainly for newcomers).
The method *Sample()* of each file is not dedicated for execution 'as is', it is rather a set of logical actions using some OCAF services.

### TDataStd_Sample.cxx

This sample contains templates for typical actions with the following standard OCAF attributes:

- Starting with data framework;
- TDataStd_Integer attribute management;
- TDataStd_RealArray attribute management;
- TDataStd_Comment attribute management;
- TDataStd_Name attribute management;
- TDataStd_UAttribute attribute management;
- TDF_Reference attribute management;
- TDataXtd_Point attribute management;
- TDataXtd_Plane attribute management;
- TDataXtd_Axis attribute management;
- TDataXtd_Geometry attribute management;
- TDataXtd_Constraint attribute management;
- TDataStd_Directory attribute management;
- TDataStd_TreeNode attribute management.



### TDocStd_Sample.cxx

This sample contains template for the following typical actions:

- creating application;
- creating the new document (document contains a framework);
- retrieving the document from a label of its framework;
- filling a document with data;
- saving a document in the file;
- closing a document;
- opening the document stored in the file;
- copying content of a document to another document with possibility to update the copy in the future.



### TPrsStd_Sample.cxx

This sample contains template for the following typical actions:

- starting with data framework;
- setting the TPrsStd_AISViewer in the framework;
- initialization of aViewer;
- finding TPrsStd_AISViewer attribute in the DataFramework;
- getting AIS_InteractiveContext from TPrsStd_AISViewer;
- adding driver to the map of drivers;
- getting driver from the map of drivers;
- setting TNaming_NamedShape to ShapeLabel;
- setting the new  TPrsStd_AISPresentation to ShapeLabel;
- displaying;
- erasing;
- updating and displaying presentation of the attribute to be displayed;
- setting a color to the displayed attribute;
- getting transparency of the displayed attribute;
- modify attribute;
- updating presentation of the attribute in viewer.



### TNaming_Sample.cxx

This sample contains template for typical actions with OCAF Topological Naming services.
The following scenario is used:

- data framework initialization;
- creating Box1 and pushing it as PRIMITIVE in DF;
- creating Box2 and pushing it as PRIMITIVE in DF;
- moving Box2 (applying a transformation);
- pushing the selected edges of the top face of Box1 in DF;
- creating a Fillet (using the selected edges) and pushing the result as a modification of Box1;
- creating a Cut (Box1, Box2) as a modification of Box1 and push it in DF;
- recovering the result from DF.

