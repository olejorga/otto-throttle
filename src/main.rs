use simconnect::{
    SimConnect_AddToDataDefinition, SimConnect_GetNextDispatch, SimConnect_MapClientEventToSimEvent, SimConnect_Open, SimConnect_RequestDataOnSimObject, SimConnect_TransmitClientEvent, DWORD, HANDLE, SIMCONNECT_DATATYPE_SIMCONNECT_DATATYPE_FLOAT64, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_PERIOD_SIMCONNECT_PERIOD_SIM_FRAME, SIMCONNECT_RECV, SIMCONNECT_RECV_ID, SIMCONNECT_RECV_ID_SIMCONNECT_RECV_ID_SIMOBJECT_DATA, SIMCONNECT_RECV_SIMOBJECT_DATA
};

use std::{ffi::CString, io, mem::transmute_copy, ptr, thread, time::Duration};

struct Event {
    id: u32,
    name: &'static str,
}

struct Variable {
    name: &'static str,
    unit: &'static str,
}

#[derive(Debug)]
struct Values {
    throttle: f64,
    speed: f64,
}

const THROTTLE_EVENT: Event = Event { id: 0, name: "THROTTLE_SET" };

const VARIABLES: [Variable; 2] = [
    Variable {
        name: "GENERAL ENG THROTTLE LEVER POSITION:1",
        unit: "Percent over 100",
    },
    Variable {
        name: "AIRSPEED INDICATED",
        unit: "Knots",
    },
];

fn main() {
    let mut client: HANDLE = ptr::null_mut();

    let name: CString = CString::new("DEMO").unwrap();

    unsafe {
        if SimConnect_Open(
            &mut client,
            name.as_ptr(),
            ptr::null_mut(),
            0,
            ptr::null_mut(),
            0,
        ) != 0
        {
            panic!("FAILED TO OPEN");
        }
    }

    for (index, variable) in VARIABLES.iter().enumerate() {
        let name: CString = CString::new(variable.name).unwrap();
        let unit: CString = CString::new(variable.unit).unwrap();

        unsafe {
            if SimConnect_AddToDataDefinition(
                client,
                0,
                name.as_ptr(),
                unit.as_ptr(),
                SIMCONNECT_DATATYPE_SIMCONNECT_DATATYPE_FLOAT64,
                0.0,
                index as u32,
            ) != 0
            {
                panic!("FAILED TO ADD DATA DEFINITION");
            }
        }
    }

    unsafe {
        if SimConnect_RequestDataOnSimObject(
            client,
            0,
            0,
            0,
            SIMCONNECT_PERIOD_SIMCONNECT_PERIOD_SIM_FRAME,
            0,
            0,
            0,
            0,
        ) != 0
        {
            panic!("FAILED TO REQUEST DATA ON SIM OBJECT");
        }
    }

    unsafe {
        let name: CString = CString::new(THROTTLE_EVENT.name).unwrap();

        if SimConnect_MapClientEventToSimEvent(
            client,
            THROTTLE_EVENT.id,
            name.as_ptr()
        ) != 0
        {
            panic!("FAILED TO MAP CLIENT EVENT TO SIM EVENT");
        }
    }

    let mut input = String::new();

    println!("Enter target speed (IAS kt):");
    io::stdin().read_line(&mut input).unwrap();

    let target: f64 = input.trim().parse().unwrap();

    let mut last_speed: f64 = 0.0;

    loop {
        let mut buffer: *mut SIMCONNECT_RECV = ptr::null_mut();
        let mut buffer_size: DWORD = 32;
        let buffer_size_ptr: *mut DWORD = &mut buffer_size;

        unsafe {
            if SimConnect_GetNextDispatch(client, &mut buffer, buffer_size_ptr) != 0 {
                continue;
            }

            match (*buffer).dwID as SIMCONNECT_RECV_ID {
                SIMCONNECT_RECV_ID_SIMCONNECT_RECV_ID_SIMOBJECT_DATA => {
                    let data: &SIMCONNECT_RECV_SIMOBJECT_DATA =
                        transmute_copy(&(buffer as *const SIMCONNECT_RECV_SIMOBJECT_DATA));
                    let values_ptr = std::ptr::addr_of!(data.dwData) as *const Values;
                    let values = std::ptr::read_unaligned(values_ptr);
                    let error: f64 = (values.speed - target).abs();
                    let mut adjustment: f64 = 0.0;

                    if (values.speed < target) && (values.speed < last_speed) {
                        if error < 5.0 {
                            adjustment = 0.0001;
                        } else {
                            adjustment = 0.001;
                        }
                    }

                    if (values.speed > target) && (values.speed > last_speed) {
                        if error < 5.0 {
                            adjustment = 0.0001;
                        } else {
                            adjustment = 0.001;
                        }
                    }

                    last_speed = values.speed;

                    let throttle =(values.throttle + adjustment).clamp(0.0, 1.0);

                    if SimConnect_TransmitClientEvent(
                        client,
                        0,
                        THROTTLE_EVENT.id,
                        (16383.0 * throttle).round() as DWORD,
                        SIMCONNECT_GROUP_PRIORITY_HIGHEST,
                        SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY
                    ) != 0
                    {
                        panic!("FAILED TO TRANSMIT CLIENT EVENT");
                    }
                }
                _ => continue,
            }

            thread::sleep(Duration::from_millis(16));
        }
    }
}
