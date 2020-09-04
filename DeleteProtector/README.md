# Delete Protector

## Initializing:

1. Modify `.inf` file:
- [Version] - Set `Class` and `ClassGuid`
- [Service] - Set `LoadOrderGroup`
- [Strings] - Set `ManufacturerName` and `Altitude`
2. Register for operations (`IRP_MJ_CREATE`, ... pre and/or post routines)
- **Don't forget to set the last callback to be `IRP_MJ_OPERATION_END` inside the `FLT_OPERATION_REGISTRATION` structure.**
3. Create the following callback functions:
- `InstanceSetup` - This is called to see if a filter would like to attach an instance to the given volume.
- `InstanceQueryTeardown` - This is called to see if the filter wants to detach from the given volume.
- `InstanceTeardownStart` - This is called at the start of a filter detaching from a volume.
- `InstanceTeardownComplete` - This is called at the end of a filter detaching from a volume.

4. Create Unload routine:
- Call `FltUnregisterFilter` - unregisters the minifilter driver's callback routines and removes any contexts that the minifilter driver has set.

After step (3) and (4) done we can use the function to create an `FLT_REGISTRATION` structure which we'll use later for registering the filter driver.

5. Implement `DriverEntry`:
- Call `FltRegisterFilter` - add the mini-filter driver to the global list of registered mini-filter drivers (and provide the Filter Manager with a list of callback functions)
- Call `FltStartFiltering` - notifies the Filter Manager that the minifilter driver is ready to begin attaching to volumes and filtering I/O requests.

## Installation:

1. Build the project
2. Copy the files to VM
3. Right-click the `.inf` file and click _install_
4. Load the driver using `fltmc load <drivername>`

