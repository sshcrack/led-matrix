import type { Property } from '~/apiTypes/list_scenes'
import { titleCase } from '~/lib/utils'

export function propertyLabel(property: Property<unknown>) {
  return property.additional?.label || titleCase(property.name)
}

export function propertyDescription(property: Property<unknown>) {
  return property.additional?.description
}

export function propertyGroup(property: Property<unknown>) {
  return property.additional?.group || 'Settings'
}

export function propertyVisible(property: Property<unknown>, args: Record<string, unknown>) {
  const condition = property.additional?.visible_if
  if (!condition) return true
  const actual = args[condition.property]
  return actual === condition.equals
}
