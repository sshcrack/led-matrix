export type Status = {
  current: string
  turned_off: boolean
  operation_mode: 'automatic' | 'manual'
  automatic_active: boolean
}
