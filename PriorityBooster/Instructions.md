# Priority Booster

## Keep In Mind

- Use _extern "C"_ above _DriverEntry_ function.
- Check **every** return status.
- Free **any** memory allocation at _Unload_.

## TODOs

- [ ] Implement _Dispatch Functions_:
  - IRP_MJ_CREATE
  - IRP_MJ_CLOSE
  - IRP_MJ_INTERNAL_DEVICE_CONTROL
- [ ] Create Device Object
- [ ] Create Symbolic link to that object
- [ ] Create shared interface between the client and the driver
- [ ] Create a client's app for calling the driver
- [ ] Implement the Boost logic
