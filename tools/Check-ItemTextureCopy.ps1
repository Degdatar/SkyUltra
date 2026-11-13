$ErrorActionPreference = 'Stop'
# Separate WARP software device, with the D3D11 debug layer when installed.
# Add-Type loads a managed helper in memory; no mod DLL/package is built or
# changed, and no Skyrim process or installed mod files are accessed.
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class SkyUltraItemTextureCopyCheck {
 [StructLayout(LayoutKind.Sequential)] struct Tex { public uint Width,Height,Mips,Array,Format,SampleCount,SampleQuality,Usage,Bind,Cpu,Misc; }
 [StructLayout(LayoutKind.Sequential)] struct View { public uint Format,Dimension,Mip,Mips,Extra0,Extra1; }
 [StructLayout(LayoutKind.Sequential)] struct Initial { public IntPtr Data; public uint Pitch,Slice; }
 [StructLayout(LayoutKind.Sequential)] struct Mapped { public IntPtr Data; public uint Pitch,Slice; }
 [StructLayout(LayoutKind.Sequential)] struct Message { public uint Category,Severity,Id; public IntPtr Text; public UIntPtr Length; }
 [DllImport("d3d11.dll")] static extern int D3D11CreateDevice(IntPtr adapter,uint driver,IntPtr software,uint flags,IntPtr levels,uint count,uint sdk,out IntPtr device,out uint chosen,out IntPtr context);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Query(IntPtr self,ref Guid iid,out IntPtr result);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int NewTexture(IntPtr self,ref Tex desc,IntPtr data,out IntPtr texture);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int NewView(IntPtr self,IntPtr resource,ref View desc,out IntPtr view);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void TextureDesc(IntPtr self,out Tex desc);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void ViewDesc(IntPtr self,out View desc);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void GetObject(IntPtr self,out IntPtr result);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Copy(IntPtr self,IntPtr destination,IntPtr source);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Map(IntPtr self,IntPtr resource,uint sub,uint mode,uint flags,out Mapped mapped);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Unmap(IntPtr self,IntPtr resource,uint sub);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate ulong MessageCount(IntPtr self);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int ReadMessage(IntPtr self,ulong index,IntPtr message,ref UIntPtr length);
 static T Method<T>(IntPtr self,int slot) where T:Delegate { return Marshal.GetDelegateForFunctionPointer<T>(Marshal.ReadIntPtr(Marshal.ReadIntPtr(self),slot*IntPtr.Size)); }
 static void Check(int hr,string operation) { if(hr<0)throw new Exception(operation+": HRESULT 0x"+hr.ToString("X8")); }
 static void Drop(ref IntPtr value) { if(value!=IntPtr.Zero){Marshal.Release(value);value=IntPtr.Zero;} }
 static void Require(bool value,string message) { if(!value)throw new Exception(message); }
 static IntPtr Duplicate(IntPtr device,IntPtr context,IntPtr source) {
  IntPtr resource=IntPtr.Zero,texture=IntPtr.Zero,copy=IntPtr.Zero,result=IntPtr.Zero;
  try {
   View view;Method<ViewDesc>(source,8)(source,out view);
   Method<GetObject>(source,7)(source,out resource);
   Guid iid=new Guid("6f15aaf2-d208-4e89-9ab4-489535d34f9c");Check(Method<Query>(resource,0)(resource,ref iid,out texture),"texture interface");
   Tex desc;Method<TextureDesc>(texture,10)(texture,out desc);
   desc.Usage=0;desc.Cpu=0;desc.Bind=8;desc.Misc&=4;
   Check(Method<NewTexture>(device,5)(device,ref desc,IntPtr.Zero,out copy),"private texture");
   Check(Method<NewView>(device,7)(device,copy,ref view,out result),"private view");
   Method<Copy>(context,47)(context,copy,texture);
   IntPtr retained=result;result=IntPtr.Zero;return retained;
  } finally { Drop(ref result);Drop(ref copy);Drop(ref texture);Drop(ref resource); }
 }
 static int Fixture(IntPtr device,IntPtr context,bool cube,bool compressed,uint viewFormat) {
  Tex desc=new Tex{Width=8,Height=8,Mips=2,Array=cube?6u:1u,Format=compressed?76u:27u,SampleCount=1,Bind=8,Misc=cube?4u:0u};
  var expected=new List<byte[]>();var memory=new List<IntPtr>();IntPtr initial=IntPtr.Zero,source=IntPtr.Zero,view=IntPtr.Zero,copyView=IntPtr.Zero,copy=IntPtr.Zero,staging=IntPtr.Zero,owner=IntPtr.Zero;
  int checkedRows=0;
  try {
   int subCount=(int)(desc.Mips*desc.Array),step=Marshal.SizeOf<Initial>();initial=Marshal.AllocHGlobal(subCount*step);
   for(int sub=0;sub<subCount;sub++) {
    int mip=sub%2,w=8>>mip,h=8>>mip,pitch=compressed?((w+3)/4)*16:w*4,rows=compressed?(h+3)/4:h;
    var bytes=new byte[pitch*rows];for(int i=0;i<bytes.Length;i++)bytes[i]=(byte)((i*7+sub*19)&255);expected.Add(bytes);
    IntPtr data=Marshal.AllocHGlobal(bytes.Length);memory.Add(data);Marshal.Copy(bytes,0,data,bytes.Length);
    Marshal.StructureToPtr(new Initial{Data=data,Pitch=(uint)pitch},IntPtr.Add(initial,sub*step),false);
   }
   Check(Method<NewTexture>(device,5)(device,ref desc,initial,out source),"source fixture");
   View vd=new View{Format=viewFormat,Dimension=cube?9u:4u,Mips=2};
   Check(Method<NewView>(device,7)(device,source,ref vd,out view),"source view");
   copyView=Duplicate(device,context,view);
   // No original model/view/texture remains. The private view must keep its
   // resource usable, with exactly the original interpretation and bytes.
   Drop(ref view);Drop(ref source);
   Method<GetObject>(copyView,3)(copyView,out owner);Require(owner==device,"Private view lost its device");Drop(ref owner);
   View actual;Method<ViewDesc>(copyView,8)(copyView,out actual);
   Require(actual.Format==viewFormat&&actual.Dimension==vd.Dimension&&actual.Mips==2,"View format/cube/mip interpretation changed");
   Method<GetObject>(copyView,7)(copyView,out copy);
   Tex read=desc;read.Usage=3;read.Bind=0;read.Cpu=0x20000;read.Misc=0;
   Check(Method<NewTexture>(device,5)(device,ref read,IntPtr.Zero,out staging),"readback fixture");
   Method<Copy>(context,47)(context,staging,copy);
   for(int sub=0;sub<subCount;sub++) {
    Mapped mapped;Check(Method<Map>(context,14)(context,staging,(uint)sub,1,0,out mapped),"readback map");
    try {
     int mip=sub%2,w=8>>mip,h=8>>mip,pitch=compressed?((w+3)/4)*16:w*4,rows=compressed?(h+3)/4:h;
     for(int y=0;y<rows;y++) {for(int x=0;x<pitch;x++)Require(Marshal.ReadByte(mapped.Data,y*(int)mapped.Pitch+x)==expected[sub][y*pitch+x],"Texture bytes/alpha/mip/cube face changed");checkedRows++;}
    } finally { Method<Unmap>(context,15)(context,staging,(uint)sub); }
   }
   Console.WriteLine("PASS: "+(cube?"cube":"2D")+" "+(compressed?"BC3":"RGBA8")+" view "+viewFormat+", all mip/face/alpha bytes intact after releasing source.");
   return checkedRows;
  } finally {
   Drop(ref owner);Drop(ref staging);Drop(ref copy);Drop(ref copyView);Drop(ref view);Drop(ref source);
   foreach(IntPtr data in memory)Marshal.FreeHGlobal(data);if(initial!=IntPtr.Zero)Marshal.FreeHGlobal(initial);
  }
 }
 public static void Run() {
  IntPtr device=IntPtr.Zero,context=IntPtr.Zero,queue=IntPtr.Zero;uint chosen;bool debug=true;
  try {
   int hr=D3D11CreateDevice(IntPtr.Zero,5,IntPtr.Zero,0x22,IntPtr.Zero,0,7,out device,out chosen,out context);
   if(hr<0){debug=false;Drop(ref context);Drop(ref device);hr=D3D11CreateDevice(IntPtr.Zero,5,IntPtr.Zero,0x20,IntPtr.Zero,0,7,out device,out chosen,out context);}
   Check(hr,"WARP device");
   Guid iid=new Guid("6543dbb6-1b48-42f5-ab82-e97ec74326f6");Method<Query>(device,0)(device,ref iid,out queue);
   int rows=0;rows+=Fixture(device,context,false,false,28);rows+=Fixture(device,context,false,false,29);
   rows+=Fixture(device,context,true,false,29);rows+=Fixture(device,context,false,true,78);rows+=Fixture(device,context,true,true,77);
   if(queue!=IntPtr.Zero) {
    ulong count=Method<MessageCount>(queue,8)(queue);
    for(ulong i=0;i<count;i++) {
     UIntPtr size=UIntPtr.Zero;Method<ReadMessage>(queue,5)(queue,i,IntPtr.Zero,ref size);IntPtr buffer=Marshal.AllocHGlobal((int)size.ToUInt64());
     try {Check(Method<ReadMessage>(queue,5)(queue,i,buffer,ref size),"debug message");Message message=Marshal.PtrToStructure<Message>(buffer);Require(message.Severity>2,"D3D11 validation: "+Marshal.PtrToStringAnsi(message.Text));}
     finally {Marshal.FreeHGlobal(buffer);}
    }
   }
   Console.WriteLine("Texture-copy lifecycle passed: 5 fixtures, "+rows+" data rows; WARP only; debug layer "+debug+"; no mod binary output.");
  } finally { Drop(ref queue);Drop(ref context);Drop(ref device); }
 }
}
'@
[SkyUltraItemTextureCopyCheck]::Run()
