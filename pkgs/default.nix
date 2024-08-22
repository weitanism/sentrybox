{ pkgs }:

{
  mass-storage-gadget =
    pkgs.writers.writePython3Bin
      "mass-storage-gadget"
      { libraries = [ ]; }
      ../mass_storage_gadget.py;

  fat32 = (pkgs.callPackage ../fat32 { });

  sentrybox-ui = (pkgs.callPackage ../ui { });
}
